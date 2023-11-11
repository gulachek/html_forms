import { cli, Path } from 'esmakefile';
import { C, platformCompiler } from 'esmakefile-c';
import { writeFile } from 'node:fs/promises';

cli((book, opts) => {
	const c = new C(platformCompiler(), {
		...opts,
		cVersion: 'C17',
		cxxVersion: 'C++20',
		book,
	});

	const config = Path.build('catui/com.gulachek.html-forms/0.1.0/config.json');

	const server = Path.src('dist/server.js');

	const bindings = Path.src('binding.gyp');
	const gypMakefile = Path.build('Makefile');
	const addon = Path.build('Release/addon.node');
	const addonSrc = Path.src('src/catui_server.cc');
	//const tsc = Path.build('tsc');

	book.add('all', [config, addon]);

	const client = c.addExecutable({
		name: 'client',
		src: ['test/client.c'],
		link: ['catui', 'msgstream'],
	});

	const cppServer = c.addExecutable({
		name: 'server',
		precompiledHeader: 'include/asio-pch.hpp',
		src: ['src/server.cpp', 'src/mime_type.cpp'],
		link: ['catui-server'],
	});

	const cmds = c.addCompileCommands();

	book.add('all', [client, cppServer, cmds]);

	book.add(config, async (args) => {
		const [cfg, srv] = args.absAll(config, server);
		await writeFile(
			cfg,
			JSON.stringify({
				exec: ['/usr/local/bin/node', srv],
			}),
			'utf8',
		);
	});

	/*
	const addonLib = c.addLibrary({
		name: 'addon',
		version: '0.1.0',
		src: ['src/catui_server.cc'],
		link: ['node'],
	});
	*/

	book.add(gypMakefile, bindings, (args) => {
		return args.spawn('node-gyp', ['configure']);
	});

	book.add(addon, [gypMakefile, addonSrc], (args) => {
		return args.spawn('node-gyp', ['build']);
	});

	/*
	book.add(tsc, (args) => {
		return args.spawn('npx', ['tsc']);
	});
	*/
});
