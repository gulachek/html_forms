import { readFile, writeFile } from 'node:fs/promises';
import { Path } from 'esmakefile';
import { basename, resolve } from 'node:path';
import { PkgConfig } from 'espkg-config';

export class Clang {
	pkg;

	constructor(make, config) {
		this.make = make;

		const extraInclude = [Path.src('include'), Path.src('private')];
		const extraCppFlags = [];
		for (const i of extraInclude) {
			extraCppFlags.push('-I', make.abs(i));
		}

		this.cc = config.cc;
		this.cxx = config.cxx;
		this.cppflags = [...extraCppFlags, ...config.cppflags];
		this.cflags = config.cflags;
		this.cxxflags = config.cxxflags;
		this.ldflags = config.ldflags;
		this.libraryType = config.libraryType;

		this.commands = [];

		this.pkg = new PkgConfig({
			searchPaths: config.pkgConfigPaths,
		});
	}

	processPkg(pkgs) {
		if (!pkgs) {
			return { pkg: [], pkgPrereqs: [] };
		}

		const pkg = [];
		const pkgPrereqs = [];
		for (const p of pkgs) {
			if (typeof p === 'string') {
				pkg.push(p);
			} else {
				pkgPrereqs.push(p);
				pkg.push(this.make.abs(p));
			}
		}

		return { pkg, pkgPrereqs };
	}

	compile(cSrc, opts) {
		const src = Path.src(cSrc);

		let compiler = this.cxx;
		let langflags = this.cxxflags;
		if (src.extname === '.c') {
			compiler = this.cc;
			langflags = this.cflags;
		}

		opts = opts || {};
		const obj = opts.out || Path.gen(src, { ext: '.o' });
		const deps = Path.gen(obj, { ext: '.d' });

		const { pkg, pkgPrereqs } = this.processPkg(opts.pkg);
		const extra = opts.extraFlags || [];

		const allFlags = async (args) => {
			let cflags = [];

			if (pkg.length > 0) {
				const result = await this.pkg.cflags(pkg);
				for (const f of result.files) {
					args.addPostreq(resolve(f));
				}

				cflags = result.flags;
			}

			return [
				'-c',
				...this.cppflags,
				...langflags,
				...cflags,
				...extra,
				'-o',
				this.make.abs(obj),
				'-MMD',
				'-MF',
				this.make.abs(deps),
				this.make.abs(src),
			];
		};

		this.commands.push({
			file: src,
			flagsFn: allFlags,
			compiler,
			prereqs: pkgPrereqs,
		});

		this.make.add(obj, [src, ...pkgPrereqs], async (args) => {
			const flags = await allFlags(args);

			const result = await args.spawn(compiler, flags);

			if (!result) return false;
			await addClangDeps(args, args.abs(deps));

			return true;
		});

		return obj;
	}

	link(opts) {
		opts = opts || {};

		if (['c', 'c++'].indexOf(opts.runtime) === -1) {
			throw new Error(
				`Invalid runtime '${opts.runtime}'. Must be 'c' or 'c++'.`,
			);
		}

		const binaries = opts.binaries;
		if (!(binaries && binaries.length > 0)) {
			throw new Error('link(): opts.binaries must be nonempty array');
		}

		const name = opts.name || basename(binaries[0].rel, binaries[0].extname);
		const linkType = opts.linkType;
		if (['static', 'shared', 'executable'].indexOf(linkType) === -1) {
			throw new Error(
				`link(): opts.linkType '${linkType}' is invalid. Must be 'static', 'shared', or 'executable'`,
			);
		}

		const { pkg, pkgPrereqs } = this.processPkg(opts.pkg);
		const pkgFlags = async (args) => {
			if (pkg.length > 0) {
				if (this.libraryType === 'static') {
					const result = await this.pkg.staticLibs(pkg);
					for (const f of result.files) args.addPostreq(resolve(f));
					return result.flags;
				} else {
					const result = await this.pkg.libs(pkg);
					for (const f of result.files) args.addPostreq(resolve(f));
					return result.flags;
				}
			}

			return [];
		};

		if (linkType === 'static') {
			const lib = Path.build(`lib${name}.a`);
			this.make.add(lib, binaries, (args) => {
				return args.spawn('ar', [
					'rcs',
					args.abs(lib),
					...args.absAll(...binaries),
				]);
			});
			return lib;
		}

		let compiler = this.cxx;
		let langflags = this.cxxflags;
		if (opts.runtime === 'c') {
			compiler = this.cc;
			langflags = this.cflags;
		}

		if (linkType === 'shared') {
			const lib = Path.build(`lib${name}.dylib`);
			this.make.add(lib, [...binaries, ...pkgPrereqs], async (args) => {
				const flags = await pkgFlags(args);

				return args.spawn(compiler, [
					...langflags,
					'-dynamiclib',
					'-o',
					args.abs(lib),
					...args.absAll(...binaries),
					...flags,
					...this.ldflags,
				]);
			});
			return lib;
		}

		const exe = Path.build(name);
		this.make.add(exe, [...binaries, ...pkgPrereqs], async (args) => {
			const flags = await pkgFlags(args);

			return args.spawn(compiler, [
				...langflags,
				'-o',
				args.abs(exe),
				...args.absAll(...binaries),
				...flags,
				...this.ldflags,
			]);
		});
		return exe;
	}

	compileCommands() {
		const p = Path.build('compile_commands.json');
		const prereqs = [];
		for (const c of this.commands) {
			prereqs.push(...c.prereqs);
		}

		this.make.add(p, prereqs, async (args) => {
			const commands = [];
			for (const c of this.commands) {
				const flags = await c.flagsFn(args);
				commands.push({
					file: args.abs(c.file),
					directory: this.make.srcRoot,
					arguments: [c.compiler, ...flags],
				});
			}

			const contents = JSON.stringify(commands);
			await writeFile(args.abs(p), contents, 'utf8');
		});

		return p;
	}
}

async function addClangDeps(args, depsAbs) {
	const depContents = await readFile(depsAbs, 'utf8');
	const escapedLines = depContents.replaceAll('\\\n', ' ');

	const lines = escapedLines.split('\n');
	if (lines.length < 1) return;

	const colonIndex = lines[0].indexOf(':');
	if (colonIndex < 0) return;

	for (const postreq of lines[0].slice(colonIndex + 1).split(/ +/)) {
		if (!postreq) continue;

		args.addPostreq(resolve(postreq));
	}
}
