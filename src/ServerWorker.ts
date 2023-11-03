import { isMainThread, parentPort, workerData } from 'node:worker_threads';
import { createRequire } from 'node:module';
const require = createRequire(import.meta.url);
const { unixRecvFd } = require('../build/Release/addon');

if (isMainThread) {
	throw new Error(
		`${__filename} is supposed to be a worker script but is running in the main thread`,
	);
}

const lbFd = workerData as number;
let running = true;
while (running) {
	const fd = unixRecvFd(lbFd);
	if (fd === -1) {
		console.error('Failed to accept catui connection');
		running = false;
	} else {
		parentPort.postMessage(fd);
	}
}
