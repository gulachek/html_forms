import { cli, Path } from 'esmakefile';
import { C, platformCompiler } from 'esmakefile-c';
import { writeFile } from 'node:fs/promises';
import { platform } from 'node:os';

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

	const cppServer = c.addExecutable({
		name: 'server',
		precompiledHeader: 'include/asio-pch.hpp',
		privateDefinitions: { ...platformDef },
		src: [
			'src/server.cpp',
			'src/mime_type.cpp',
			'src/http_listener.cpp',
			'src/open-url.cpp',
		],
		link: ['catui-server', htmlLib],
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
