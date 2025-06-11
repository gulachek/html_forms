import { cli, Path } from 'esmakefile';
import { Distribution, addCompileCommands } from 'esmakefile-cmake';
import { writeFile, readFile } from 'node:fs/promises';
import { platform } from 'node:os';
import { addWebpack } from './WebpackRule.mjs';

const packageContent = await readFile('package.json', 'utf8');
const { version } = JSON.parse(packageContent);

const config = {
	webpack: {
		devtool: 'inline-source-map',
		mode: 'development',
	},
};

cli((make) => {
	make.add('all', []);

	const { htmlLib, distClient, example } = makeClient(make);
	const { serverLib, distServer, testServer } = makeServer(make, htmlLib);

	const doxygen = Path.build('docs/html/index.html');
	make.add(doxygen, ['Doxyfile', 'client/include/html_forms.h'], (args) => {
		return args.spawn('doxygen');
	});

	make.add('all', [
		doxygen,
		serverLib.binary,
		htmlLib.binary,
		testServer.binary,
		example,
		addCompileCommands(make, distClient, distServer),
	]);
});

function makeClient(make) {
	const d = new Distribution(make, {
		name: 'html_forms',
		version,
		cStd: 17,
		cxxStd: 20,
	});

	const catui = findCatui(d);
	const cjson = findCjson(d);
	const gtest = findGtest(d);
	const boost = findBoost(d);

	const htmlLib = d.addLibrary({
		name: 'html_forms',
		src: ['client/src/html_forms.c'],
		includeDirs: ['client/include'],
		linkTo: [cjson, catui],
	});

	const mimeConfig = makeConfig(make, 'example/mime_swap/config.c', {
		docroot: Path.src('example/mime_swap/docroot'),
	});

	const mimeSwap = d.addTest({
		name: 'mime_swap',
		src: ['example/mime_swap/main.c', mimeConfig],
		linkTo: [htmlLib],
	});

	const inputTransferConfig = makeConfig(
		make,
		'example/input_transfer/config.c',
		{
			docroot: Path.src('example/input_transfer/docroot'),
		},
	);

	const inputTransfer = d.addTest({
		name: 'input_transfer',
		src: ['example/input_transfer/main.c', inputTransferConfig],
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

	const tarball = d.addTest({
		name: 'tarball',
		src: ['example/tarball/main.c', tarballConfig],
		linkTo: [htmlLib],
	});

	const todoConfig = makeConfig(make, 'example/todo/config.c', {
		docroot: Path.src('example/todo/docroot'),
	});

	const todo = d.addTest({
		name: 'todo',
		src: ['example/todo/main.c', todoConfig],
		linkTo: [htmlLib],
	});

	const loadingConfig = makeConfig(make, 'example/loading/config.cpp', {
		docroot: Path.src('example/loading/docroot'),
	});

	const loading = d.addTest({
		name: 'loading',
		src: ['example/loading/main.cpp', loadingConfig],
		linkTo: [htmlLib],
	});

	const snakeConfig = makeConfig(make, 'example/snake/config.cpp', {
		docroot: Path.src('example/snake/docroot'),
	});

	const snake = d.addTest({
		name: 'snake',
		src: ['example/snake/main.cpp', snakeConfig],
		linkTo: [boost, htmlLib],
	});

	const example = Path.build('example');
	make.add(
		example,
		[
			loading.binary,
			snake.binary,
			tarball.binary,
			todo.binary,
			mimeSwap.binary,
			inputTransfer.binary,
		],
		() => {},
	);

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

	make.add('test', [parseFormTest.run, escapeStringTest.run]);

	return { htmlLib, distClient: d, example };
}

function makeServer(make, htmlLib) {
	const d = new Distribution(make, {
		name: 'html_forms_server',
		version,
		cStd: 17,
		cxxStd: 20,
	});

	const catui = findCatui(d);
	const cjson = findCjson(d);
	const libarchive = findLibArchive(d);
	// this is needed because libarchive cmake package is
	// leaky and does not find zlib
	const zlib = findZlib(d);
	const gtest = findGtest(d);
	const boost = findBoost(d);

	const formsTs = Path.src('server/src/forms.ts');
	const browserTs = Path.src('test/browser.ts');

	const wpDir = Path.build('webpack');
	const formsJsBundle = wpDir.join('forms.js');

	addWebpack(make, config.webpack, formsTs, formsJsBundle, {
		target: 'electron-main',
		output: {
			library: {
				type: 'var',
				name: 'HtmlForms',
			},
		},
	});

	const formsJsCpp = Path.build('server/src/forms_js.cpp');
	make.add(formsJsCpp, [formsJsBundle], async (args) => {
		const [cpp, js] = args.absAll(formsJsCpp, formsJsBundle);

		const buf = await readFile(js);
		await writeFile(cpp, bufToCppArray('forms_js', buf), 'utf8');
	});

	const loadingHtml = Path.src('server/src/loading.html');
	const loadingHtmlCpp = Path.build('server/src/loading_html.cpp');
	make.add(loadingHtmlCpp, [loadingHtml], async (args) => {
		const [cpp, html] = args.absAll(loadingHtmlCpp, loadingHtml);

		const buf = await readFile(html);
		await writeFile(cpp, bufToCppArray('loading_html', buf), 'utf8');
	});

	let session_lock;
	if (platform() === 'darwin') {
		session_lock = Path.src('server/src/posix/session_lock.cpp');
	} else {
		throw new Error('Platform not supported');
	}

	const serverLib = d.addLibrary({
		name: 'html_forms_server',
		src: [
			'server/src/server.cpp',
			'server/src/mime_type.cpp',
			'server/src/http_listener.cpp',
			'server/src/my-asio.cpp',
			'server/src/my-beast.cpp',
			'server/src/browser.cpp',
			'server/src/parse_target.cpp',
			session_lock,
			formsJsCpp,
			loadingHtmlCpp,
		],
		linkTo: [htmlLib, libarchive, zlib, boost, catui],
		includeDirs: ['server/include'],
	});

	const browserBundle = wpDir.join('browser.cjs');
	addWebpack(make, config.webpack, browserTs, browserBundle, {
		target: 'electron-main',
	});

	const testServerConfig = makeConfig(make, 'test/test_server_config.cpp', {
		browser_bundle: browserBundle,
	});

	make.add(testServerConfig, [browserBundle]);

	const testServer = d.addTest({
		name: 'test_server',
		src: ['test/test_server.cpp', testServerConfig],
		linkTo: [cjson, serverLib],
	});

	const urlTest = d.addTest({
		name: 'url_test',
		src: ['test/url_test.cpp'],
		linkTo: [serverLib, gtest],
	});

	const formsTestConfig = makeConfig(make, 'test/forms_test_config.cpp', {
		test_scratch_dir: Path.build('.scratch'),
		content_dir: Path.src('test/content'),
	});

	const formsTest = d.addTest({
		name: 'forms_test',
		src: ['test/forms_test.cpp', formsTestConfig],
		linkTo: [htmlLib, serverLib, gtest, boost],
	});

	make.add('test', [urlTest.run, formsTest.run], () => {});

	return { serverLib, distServer: d, testServer };
}

function findGtest(dist) {
	return dist.findPackage('gtest_main');
}

function findCatui(dist) {
	return dist.findPackage('catui');
}

function findCjson(dist) {
	return dist.findPackage({
		pkgconfig: 'libcjson',
		cmake: {
			packageName: 'cJSON',
			libraryTarget: 'cjson',
		},
	});
}

function findLibArchive(dist) {
	return dist.findPackage({
		pkgconfig: 'libarchive',
		cmake: {
			packageName: 'LibArchive',
			libraryTarget: 'LibArchive::LibArchive',
		},
	});
}

function findZlib(dist) {
	return dist.findPackage({
		pkgconfig: 'zlib',
		cmake: {
			packageName: 'ZLIB',
			libraryTarget: 'ZLIB::ZLIB',
		},
	});
}

function findBoost(dist) {
	return dist.findPackage({
		pkgconfig: 'boost-headers',
		cmake: {
			packageName: 'Boost',
			component: 'headers',
			libraryTarget: 'Boost::headers',
		},
	});
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
