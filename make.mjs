import { cli, Path } from 'esmakefile';
import { Distribution, addCompileCommands } from 'esmakefile-cmake';
import { writeFile, readFile } from 'node:fs/promises';
import { platform } from 'node:os';
import { addWebpack } from './WebpackRule.mjs';

const config = {
	webpack: {
		devtool: 'inline-source-map',
		mode: 'development',
	},
};

cli((make) => {
	make.add('all', []);

	const d = new Distribution(make, {
		name: 'html_forms',
		version: '0.1.0',
		cStd: 17,
		cxxStd: 20,
	});

	const catui = d.findPackage('catui');
	const cjson = d.findPackage({
		pkgconfig: 'libcjson',
		cmake: {
			packageName: 'cJSON',
			libraryTarget: 'cjson',
		},
	});
	const boost = d.findPackage({
		pkgconfig: 'boost-headers',
		cmake: {
			packageName: 'Boost',
			component: 'headers',
			libraryTarget: 'Boost::headers',
		},
	});
	const libarchive = d.findPackage('libarchive');

	const gtest = d.findPackage('gtest_main');

	//const sqlite3 = d.findPackage('sqlite3');

	const htmlLib = d.addLibrary({
		name: 'html_forms',
		src: ['src/client/html_forms.c'],
		includeDirs: ['include', 'private'],
		linkTo: [cjson, catui],
	});

	const mimeSwap = d.addTest({
		name: 'mime_swap',
		src: ['example/mime_swap/main.c'],
		linkTo: [htmlLib],
	});

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

	const tarballConfig = makeConfig(make, 'example/tarball/config.c', {
		tarball_path: tarballArchive,
	});

	make.add(tarballConfig, [tarballArchive]);

	const tarball = d.addExecutable({
		name: 'tarball',
		src: ['example/tarball/main.c', tarballConfig],
		linkTo: [htmlLib],
	});

	const todo = d.addTest({
		name: 'todo',
		src: ['example/todo/main.c'],
		linkTo: [htmlLib /*, sqlite3*/],
		//`-DDOCROOT_PATH="${make.abs(Path.src('example/todo/docroot'))}"`,
	});

	const loadingConfig = makeConfig(make, 'example/loading/config.cpp', {
		docroot: Path.src('example/loading/docroot'),
	});

	const loading = d.addExecutable({
		name: 'loading',
		src: ['example/loading/main.cpp', loadingConfig],
		linkTo: [htmlLib],
	});

	const snakeConfig = makeConfig(make, 'example/snake/config.cpp', {
		docroot: Path.src('example/snake/docroot'),
	});

	const snake = d.addExecutable({
		name: 'snake',
		src: ['example/snake/main.cpp', snakeConfig],
		linkTo: [boost, htmlLib],
	});

	/*
	const example = Path.build('example');
	make.add(example, [mimeSwap, tarball, tarballArchive, todo, snake, loading]);
	*/
	const example = Path.build('example');
	make.add(example, [loading.binary, snake.binary, tarball.binary], () => {});

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

	const serverLib = d.addLibrary({
		name: 'html_forms_server',
		src: [
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
		],
		linkTo: [htmlLib, libarchive, boost],
		includeDirs: ['include', 'private'],
	});

	const testServer = d.addExecutable({
		name: 'test_server',
		src: ['test/test_server.cpp'],
		linkTo: [cjson, serverLib],
	});

	const urlTest = d.addTest({
		name: 'url_test',
		src: ['test/url_test.cpp'],
		linkTo: [serverLib, gtest],
	});

	const parseFormTest = d.addTest({
		name: 'parse_form_test',
		src: ['test/parse_form_test.cpp'],
		linkTo: [htmlLib, gtest],
	});

	const escapeStringTest = d.addTest({
		name: 'escape_string_test',
		src: ['test/escape_string_test.cpp'],
		linkTo: [htmlLib, gtest],
	});

	const testSock = Path.build('catui.sock');
	const catuiDir = Path.build('catui');
	const contentDir = Path.src('test/content');

	const formsTest = d.addTest({
		name: 'forms_test',
		src: ['test/forms_test.cpp'],
		linkTo: [htmlLib /* test framework */],
		/*
						CATUI_ADDRESS: testSock,
						CATUI_DIR: catuiDir,
						CONTENT_DIR: contentDir,
		 */
	});

	make.add(
		'test',
		[urlTest.run, parseFormTest.run, escapeStringTest.run],
		() => {},
	);

	const doxygen = Path.build('docs/html/index.html');
	make.add(doxygen, ['Doxyfile', 'include/html_forms.h'], (args) => {
		return args.spawn('doxygen');
	});

	make.add('all', [
		browserBundle,
		doxygen,
		serverLib.binary,
		htmlLib.binary,
		testServer.binary,
		example,
		addCompileCommands(make, d),
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

function makeConfig(make, configPath, keyVals) {
	const lines = [];
	for (const key in keyVals) {
		let val = keyVals[key];
		if (Path.isPath(val)) {
			val = make.abs(val);
		}

		lines.push(`const char *${key} = "${val}";`);
	}

	const config = Path.build(configPath);
	make.add(config, async (args) => {
		await writeFile(args.abs(config), lines.join('\n'), 'utf8');
	});

	return config;
}
