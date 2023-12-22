import { cli, Path } from 'esmakefile';
import { C, platformCompiler } from 'esmakefile-c';
import { writeFile, readFile } from 'node:fs/promises';
import { platform } from 'node:os';
import { addWebpack } from './WebpackRule.mjs';

cli((book, opts) => {
	const c = new C(platformCompiler(), {
		...opts,
		cVersion: 'C17',
		cxxVersion: 'C++20',
		book,
	});

	const config = Path.build('catui/com.gulachek.html-forms/0.1.0/config.json');

	book.add('all', [config]);

	const htmlLib = c.addLibrary({
		name: 'html-forms',
		privateDefinitions: {
			HTML_API: 'EXPORT',
		},
		privateIncludes: ['private'],
		src: ['src/html-forms.c'],
		link: ['msgstream', 'cjson', 'catui'],
	});

	const client = c.addExecutable({
		name: 'client',
		src: ['test/client.c'],
		link: ['msgstream', htmlLib],
	});

	const mimeSwap = c.addExecutable({
		name: 'mime_swap',
		src: ['example/mime_swap/main.c'],
		link: [htmlLib],
	});

	const tarballIndexHtml = Path.src('example/tarball/docroot/index.html');
	const tarballMainCss = Path.src('example/tarball/docroot/style/main.css');

	const tarballArchive = Path.build('example/tarball.tar.gz');
	book.add(tarballArchive, [tarballIndexHtml, tarballMainCss], async (args) => {
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

	const tarball = c.addExecutable({
		name: 'tarball',
		src: ['example/tarball/main.c'],
		privateDefinitions: {
			TARBALL_PATH: `"${book.abs(tarballArchive)}"`,
		},
		link: [htmlLib],
	});

	const todo = c.addExecutable({
		name: 'todo',
		src: ['example/todo/main.c'],
		privateDefinitions: {
			DOCROOT_PATH: `"${book.abs(Path.src('example/todo/docroot'))}"`,
		},
		link: [htmlLib, 'sqlite3'],
	});

	const snake = c.addExecutable({
		name: 'snake',
		src: ['example/snake/main.cpp'],
		privateDefinitions: {
			DOCROOT_PATH: `"${book.abs(Path.src('example/snake/docroot'))}"`,
		},
		link: [htmlLib, 'boost-json'],
	});

	const example = Path.build('example');
	book.add(example, [mimeSwap, tarball, tarballArchive, todo, snake]);

	const platformDef = {};
	switch (platform()) {
		case 'darwin':
			platformDef.PLATFORM_MACOS = 1;
			break;
		default:
			break;
	}

	const formsTs = Path.src('src/forms.ts');
	const browserTs = Path.src('src/browser.ts');

	const wpDir = Path.build('webpack');
	const formsJsBundle = wpDir.join('forms.js');
	const browserBundle = wpDir.join('browser.cjs');

	addWebpack(book, opts, formsTs, formsJsBundle, {
		target: 'electron-main',
		output: {
			library: {
				type: 'var',
				name: 'HtmlForms',
			},
		},
	});
	addWebpack(book, opts, browserTs, browserBundle, { target: 'electron-main' });

	const formsJsCpp = Path.build('forms_js.cpp');
	book.add(formsJsCpp, [formsJsBundle], async (args) => {
		const [cpp, js] = args.absAll(formsJsCpp, formsJsBundle);

		const buf = await readFile(js);
		await writeFile(cpp, bufToCppArray('forms_js', buf), 'utf8');
	});

	const cppServer = c.addExecutable({
		name: 'server',
		precompiledHeader: 'include/asio-pch.hpp',
		privateDefinitions: {
			...platformDef,
			BROWSER_EXE: '"npx"',
			BROWSER_ARGS: `{"electron", "${book.abs(browserBundle)}"}`,
		},
		src: [
			'src/server.cpp',
			'src/mime_type.cpp',
			'src/http_listener.cpp',
			'src/open-url.cpp',
			'src/my-asio.cpp',
			'src/my-beast.cpp',
			'src/browser.cpp',
			formsJsCpp,
		],
		link: ['catui', htmlLib, 'boost-json', 'boost-filesystem', 'libarchive'],
	});

	const urlTest = c.addExecutable({
		name: 'url_test',
		src: ['test/url_test.cpp'],
		link: ['boost-unit_test_framework', htmlLib, 'msgstream'],
	});

	const parseFormTest = c.addExecutable({
		name: 'parse-form-test',
		src: ['test/parse-form-test.cpp'],
		privateIncludes: ['private'],
		link: ['boost-unit_test_framework', htmlLib, 'msgstream'],
	});

	const escapeStringTest = c.addExecutable({
		name: 'escape_string_test',
		src: ['test/escape_string_test.cpp'],
		privateIncludes: ['private'],
		link: ['boost-unit_test_framework', htmlLib],
	});

	const tests = [urlTest, parseFormTest, escapeStringTest];
	const runTests = tests.map((t) => t.dir().join(t.basename + '.run'));

	for (let i = 0; i < tests.length; ++i) {
		book.add(runTests[i], [tests[i]], (args) => {
			return args.spawn(args.abs(tests[i]));
		});
	}

	const test = Path.build('test');
	book.add(test, runTests);

	const cmds = c.addCompileCommands();

	book.add('all', [client, cppServer, cmds, test, browserBundle, example]);

	book.add(config, async (args) => {
		const [cfg, srv] = args.absAll(config, cppServer);
		await writeFile(
			cfg,
			JSON.stringify({
				exec: [srv, '3838'],
			}),
			'utf8',
		);
	});
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
