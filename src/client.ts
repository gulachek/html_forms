import { createRequire } from 'node:module';
import { connect, Socket } from 'node:net';
import { open } from 'node:fs/promises';

const require = createRequire(import.meta.url);
const { unixSendFd } = require('../build/Release/addon');

function connectAsync(path: string): Promise<Socket> {
	let res: Function;
	const prom = new Promise<Socket>((resolve) => {
		res = resolve;
	});
	const sock = connect(path, () => {
		res(sock);
	});
	return prom;
}

function getFd(sock: Socket): number {
	return (sock as any)._handle.fd;
}

async function main() {
	const sock = await connectAsync('test.sock');
	const f = await open('binding.gyp', 'r');
	unixSendFd(getFd(sock), f.fd);
	f.close();
}

main();
