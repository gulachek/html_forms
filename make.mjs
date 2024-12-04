import { cli, Path } from 'esmakefile';
import { writeFile, readFile } from 'node:fs/promises';
import { platform } from 'node:os';
import { addWebpack } from './WebpackRule.mjs';
import { Clang } from './jslib/clang.mjs';
import Ajv from 'ajv';

const ajv = new Ajv();
const stringArray = { type: 'array', items: { type: 'string' } };
const schema = {
	type: 'object',
	properties: {
		cc: { type: 'string' },
		cxx: { type: 'string' },
		cppflags: { ...stringArray },
		cflags: { ...stringArray },
		cxxflags: { ...stringArray },
		ldflags: { ...stringArray },
		pkgConfigPaths: { ...stringArray, uniqueItems: true },
		libraryType: { type: 'string', enum: ['static', 'shared'] },
		webpack: {
			type: 'object',
			properties: {
				mode: { type: 'string', enum: ['development', 'production'] },
				devtool: { type: 'string' },
			},
		},
	},
	additionalProperties: false,
};

const validate = ajv.compile(schema);

const inputConfig = JSON.parse(await readFile('config.json', 'utf8'));
if (!validate(inputConfig)) {
	console.error('The configuration file "config.json" has errors.');
	console.error(validate.errors);
	process.exit(1);
}

const config = {
	cc: 'clang',
	cxx: 'clang++',
	cppflags: [],
	cflags: [
		'-std=c17',
		//'-fvisibility=hidden',
		//'-DCATUI_API=__attribute__((visibility("default")))',
	],
	cxxflags: ['-std=c++20'],
	ldflags: [],
	pkgConfigPaths: ['pkgconfig'],
	libraryType: 'static',
	webpack: {
		devtool: 'inline-src-map',
		mode: 'development',
	},
	...inputConfig, // override the defaults
};

cli((make) => {
	const pc = Path.build('pkgconfig');
	const htmlFormsPc = pc.join('html_forms.pc');

	const clang = new Clang(make, config);

	const catuiConfig = Path.build(
		'catui/com.gulachek.html-forms/0.1.0/config.json',
	);

	make.add('all', [catuiConfig]);

	const htmlFormsPkg = ['msgstream', 'cjson', 'catui', 'unixsocket'];
	const htmlLib = clang.link({
		name: 'html_forms',
		runtime: 'c',
		linkType: config.libraryType,
		pkg: htmlFormsPkg,
		binaries: [clang.compile('src/client/html_forms.c', { pkg: htmlFormsPkg })],
	});
	make.add(htmlFormsPc, async (args) => {
		await writeFile(
			args.abs(htmlFormsPc),
			`
		builddir=\${pcfiledir}/..
		Name: html_forms
		Version: 0.1.0
		Description: 
		Requires.private: ${htmlFormsPkg.join(' ')}
		Cflags:
		Libs: -L\${builddir} -lhtml_forms
		`,
		);
	});

	const mimeSwap = clang.link({
		name: 'mime_swap',
		runtime: 'c',
		linkType: 'executable',
		binaries: [
			clang.compile('example/mime_swap/main.c', { pkg: [htmlFormsPc] }),
		],
		pkg: [htmlFormsPc],
	});
	make.add(mimeSwap, [htmlLib]);

	const tarballIndexHtml = Path.src('example/tarball/docroot/index.html');
	const tarballMainCss = Path.src('example/tarball/docroot/style/main.css');

	const tarballArchive = Path.build('example/tarball.tar.gz');
	make.add(tarballArchive, [tarballIndexHtml, tarballMainCss], async (args) => {
		const [src, tar] = args.absAll(
			Path.src('example/tarball/docroot'),
			tarballArchive,
		);

		// make all paths relative to docroot
		return args.spawn('tar', [
			'-c',
			'-z',
			'-f',
			tar,
			'-C',
			src,
			'--strip-components',
			'1',
			'.',
		]);
	});

	const tarball = clang.link({
		name: 'tarball',
		runtime: 'c',
		linkType: 'executable',
		binaries: [
			clang.compile('example/tarball/main.c', {
				extraFlags: [`-DTARBALL_PATH="${make.abs(tarballArchive)}"`],
			}),
			htmlLib,
		],
		pkg: [htmlFormsPc],
	});
	make.add(tarball, [htmlLib]);

	const todo = clang.link({
		name: 'todo',
		runtime: 'c',
		linkType: 'executable',
		binaries: [
			clang.compile('example/todo/main.c', {
				extraFlags: [
					`-DDOCROOT_PATH="${make.abs(Path.src('example/todo/docroot'))}"`,
				],
				pkg: [htmlFormsPc, 'sqlite3'],
			}),
		],
		pkg: [htmlFormsPc, 'sqlite3'],
	});
	make.add(todo, [htmlLib]);

	const loading = clang.link({
		name: 'loading',
		runtime: 'c++',
		linkType: 'executable',
		binaries: [
			clang.compile('example/loading/main.cpp', {
				pkg: [htmlFormsPc],
				extraFlags: [
					`-DDOCROOT_PATH="${make.abs(Path.src('example/loading/docroot'))}"`,
				],
			}),
		],
		pkg: [htmlFormsPc],
	});

	const snake = clang.link({
		name: 'snake',
		runtime: 'c++',
		linkType: 'executable',
		binaries: [
			clang.compile('example/snake/main.cpp', {
				pkg: [htmlFormsPc, 'boost-json'],
				extraFlags: [
					`-DDOCROOT_PATH="${make.abs(Path.src('example/snake/docroot'))}"`,
				],
			}),
		],
		pkg: [htmlFormsPc, 'boost-json'],
	});

	const example = Path.build('example');
	make.add(example, [mimeSwap, tarball, tarballArchive, todo, snake, loading]);

	const formsTs = Path.src('src/server/forms.ts');
	const browserTs = Path.src('test/browser.ts');

	const wpDir = Path.build('webpack');
	const formsJsBundle = wpDir.join('forms.js');
	const browserBundle = wpDir.join('browser.cjs');

	addWebpack(make, config.webpack, formsTs, formsJsBundle, {
		target: 'electron-main',
		output: {
			library: {
				type: 'var',
				name: 'HtmlForms',
			},
		},
	});
	addWebpack(make, config.webpack, browserTs, browserBundle, {
		target: 'electron-main',
	});

	const formsJsCpp = Path.build('server/forms_js.cpp');
	make.add(formsJsCpp, [formsJsBundle], async (args) => {
		const [cpp, js] = args.absAll(formsJsCpp, formsJsBundle);

		const buf = await readFile(js);
		await writeFile(cpp, bufToCppArray('forms_js', buf), 'utf8');
	});

	const loadingHtml = Path.src('src/server/loading.html');
	const loadingHtmlCpp = Path.build('server/loading_html.cpp');
	make.add(loadingHtmlCpp, [loadingHtml], async (args) => {
		const [cpp, html] = args.absAll(loadingHtmlCpp, loadingHtml);

		const buf = await readFile(html);
		await writeFile(cpp, bufToCppArray('loading_html', buf), 'utf8');
	});

	let session_lock;
	if (platform() === 'darwin') {
		session_lock = Path.src('src/server/posix/session_lock.cpp');
	} else {
		throw new Error('Platform not supported');
	}

	const cppServerPkg = [
		'catui',
		'boost-json',
		'boost-filesystem',
		'libarchive',
		htmlFormsPc,
	];

	const cppServerBin = [
		'src/server/server.cpp',
		'src/server/mime_type.cpp',
		'src/server/http_listener.cpp',
		'src/server/my-asio.cpp',
		'src/server/my-beast.cpp',
		'src/server/browser.cpp',
		'src/server/parse_target.cpp',
		session_lock,
		formsJsCpp,
		loadingHtmlCpp,
	].map((s) => {
		return clang.compile(s, {
			pkg: cppServerPkg,
			extraFlags: ['-DPLATFORM_MACOS=1'],
		});
	});

	const cppServer = clang.link({
		name: 'html_forms_server',
		runtime: 'c++',
		linkType: config.libraryType,
		// Somehow has a bug where the wrong boost-json header is
		// included.
		//precompiledHeader: 'private/asio-pch.hpp',
		binaries: [...cppServerBin],
		pkg: cppServerPkg,
	});
	make.add(cppServer, [htmlLib]);
	const serverPc = Path.build('pkgconfig/html_forms_server.pc');
	const requiresPrivate = cppServerPkg
		.map((p) => (typeof p === 'string' ? p : make.abs(p)))
		.join(' ');
	make.add(serverPc, async (args) => {
		await writeFile(
			args.abs(serverPc),
			`
		builddir=\${pcfiledir}/..
		Name: html_forms_server
		Version: 0.1.0
		Description: 
		Requires.private: ${requiresPrivate}
		Cflags:
		Libs: -L\${builddir} -lhtml_forms_server #-lc++?
		`,
		);
	});

	const testServer = clang.link({
		name: 'test_server',
		runtime: 'c++',
		linkType: 'executable',
		binaries: [
			clang.compile('test/test_server.cpp', {
				pkg: [serverPc, 'unixsocket', 'catui', 'msgstream', 'boost-json'],
				extraFlags: [
					`-DBROWSER_EXE="npx"`,
					`-DBROWSER_ARGS={"npx", "electron", "${make.abs(browserBundle)}"}`,
				],
			}),
		],
		pkg: [serverPc, 'unixsocket', 'catui', 'msgstream', 'boost-json'],
	});
	make.add(testServer, [cppServer, htmlFormsPc]);

	const urlTest = clang.link({
		name: 'url_test',
		runtime: 'c++',
		linkType: 'executable',
		pkg: ['boost-unit_test_framework', htmlFormsPc, 'msgstream'],
		binaries: [
			clang.compile('test/url_test.cpp', {
				pkg: ['boost-unit_test_framework', htmlFormsPc, 'msgstream'],
			}),
			Path.build('src/server/parse_target.o'),
		],
	});
	make.add(urlTest, [htmlLib]);

	const parseFormTest = clang.link({
		name: 'parse-form-test',
		runtime: 'c++',
		linkType: 'executable',
		pkg: ['boost-unit_test_framework', htmlFormsPc, 'msgstream'],
		binaries: [
			clang.compile('test/parse-form-test.cpp', {
				pkg: ['boost-unit_test_framework', htmlFormsPc, 'msgstream'],
			}),
		],
	});
	make.add(parseFormTest, [htmlLib]);

	const escapeStringTest = clang.link({
		name: 'escape_string_test',
		runtime: 'c++',
		linkType: 'executable',
		pkg: ['boost-unit_test_framework', htmlFormsPc],
		binaries: [
			clang.compile('test/escape_string_test.cpp', {
				pkg: ['boost-unit_test_framework', htmlFormsPc],
			}),
		],
	});
	make.add(escapeStringTest, [htmlLib]);

	const testSock = Path.build('catui.sock');
	const catuiDir = Path.build('catui');
	const contentDir = Path.src('test/content');

	const formsTest = clang.link({
		name: 'forms_test',
		runtime: 'c++',
		linkType: 'executable',
		pkg: ['boost-unit_test_framework', htmlFormsPc],
		binaries: [
			clang.compile('test/forms_test.cpp', {
				pkg: ['boost-unit_test_framework', htmlFormsPc],
				extraFlags: define(
					{
						CATUI_ADDRESS: testSock,
						CATUI_DIR: catuiDir,
						CONTENT_DIR: contentDir,
					},
					make,
				),
			}),
		],
	});
	make.add(formsTest, [htmlLib]);

	const tests = [urlTest, parseFormTest, escapeStringTest];
	const runTests = tests.map((t) => t.dir().join(t.basename + '.run'));

	for (let i = 0; i < tests.length; ++i) {
		make.add(runTests[i], [tests[i]], (args) => {
			return args.spawn(args.abs(tests[i]));
		});
	}

	const test = Path.build('test');
	make.add(test, runTests);

	const doxygen = Path.build('docs/html/index.html');
	make.add(doxygen, ['Doxyfile', 'include/html_forms.h'], (args) => {
		return args.spawn('doxygen');
	});

	const fastTests = [
		'url_test.run',
		'parse-form-test.run',
		'escape_string_test.run',
	].map((p) => Path.build(p));

	make.add(catuiConfig, async (args) => {
		const [cfg, srv] = args.absAll(catuiConfig, cppServer);
		await writeFile(
			cfg,
			JSON.stringify({
				exec: [srv, '3838'],
			}),
			'utf8',
		);
	});

	make.add('all', [
		clang.compileCommands(),
		testServer,
		tarball,
		tarballArchive,
		mimeSwap,
		todo,
		loading,
		snake,
		browserBundle,
		doxygen,
		formsTest,
		...fastTests,
	]);
});

function bufToCppArray(identifier, buf) {
	const array = `${identifier}_array__`;

	const pieces = [
		`#include <span>
		#include <cstdint>

		const std::uint8_t ${array}[${buf.length}] = {`,
	];

	if (buf.length > 0) pieces.push(buf[0]);

	for (let i = 1; i < buf.length; ++i) {
		pieces.push(',', buf[i]);
	}

	pieces.push(`};
	std::span<const std::uint8_t> ${identifier}() {
		return std::span<const std::uint8_t>{${array}, ${buf.length}};
	}`);

	return pieces.join('');
}

function define(obj, make) {
	const flags = [];

	for (let key in obj) {
		const val = obj[key];
		if (val) {
			if (typeof val.rel === 'function') {
				flags.push(`-D${key}="${make.abs(val)}"`);
			} else {
				flags.push(`-D${key}=${val}`);
			}
		} else {
			flags.push(`-D${key}`);
		}
	}

	return flags;
}
