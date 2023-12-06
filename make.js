import { cli, Path } from 'esmakefile';
import { C, platformCompiler } from 'esmakefile-c';
import { writeFile, readFile } from 'node:fs/promises';
import { platform } from 'node:os';
import webpack from 'webpack';
import { dirname, basename } from 'node:path';

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
		src: ['src/html-forms.c'],
		link: ['msgstream', 'cjson', 'catui'],
	});

	const client = c.addExecutable({
		name: 'client',
		src: ['test/client.c'],
		link: ['catui', 'msgstream', htmlLib],
	});

	const platformDef = {};
	switch (platform()) {
		case 'darwin':
			platformDef.PLATFORM_MACOS = 1;
			break;
		default:
			break;
	}

	const formsJs = Path.src('src/forms.js');
	const formsJsBundle = Path.build('forms.js');
	let wp;

	book.add(formsJsBundle, [formsJs], (args) => {
		const [src, bundle] = args.absAll(formsJs, formsJsBundle);

		const config = {
			entry: src,
			output: {
				path: dirname(bundle),
				filename: basename(bundle),
			},
		};

		if (opts.isDevelopment) {
			config.mode = 'development';
			config.devtool = 'inline-source-map';
		}

		wp = wp || webpack(config);

		return new Promise((res) => {
			wp.run((err, stats) => {
				err && args.logStream.write(err);
				args.logStream.write(stats.toString({ colors: true }));
				if (err || stats.hasErrors()) {
					res(false);
				}

				wp.close((closeErr) => {
					console.error('Closing webpack', closeErr);
				});
			});
		});
	});

	const formsJsCpp = Path.build('forms_js.cpp');
	book.add(formsJsCpp, [formsJsBundle], async (args) => {
		const [cpp, js] = args.absAll(formsJsCpp, formsJsBundle);

		const buf = await readFile(js);
		await writeFile(cpp, bufToCppArray('forms_js', buf), 'utf8');
	});

	const cppServer = c.addExecutable({
		name: 'server',
		precompiledHeader: 'include/asio-pch.hpp',
		privateDefinitions: { ...platformDef },
		src: [
			'src/server.cpp',
			'src/mime_type.cpp',
			'src/http_listener.cpp',
			'src/open-url.cpp',
			'src/my-asio.cpp',
			'src/my-beast.cpp',
			formsJsCpp,
		],
		link: ['catui-server', htmlLib, 'boost-json'],
	});

	const urlTest = c.addExecutable({
		name: 'url_test',
		src: ['test/url_test.cpp'],
		link: ['boost-unit_test_framework', htmlLib, 'msgstream'],
	});

	const test = Path.build('test');
	book.add(test, [urlTest], (args) => {
		return args.spawn(args.abs(urlTest));
	});

	const cmds = c.addCompileCommands();

	book.add('all', [client, cppServer, cmds, test]);

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
