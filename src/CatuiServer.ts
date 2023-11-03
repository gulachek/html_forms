import { join, dirname } from 'node:path';
import { Socket } from 'node:net';
import { MsgStreamSocket } from './MsgStream.js';
import { Worker } from 'node:worker_threads';
import { close } from 'node:fs';

export type ConnectListener = (sock: Socket) => void;

export class CatuiServer {
	private static _inst: CatuiServer | null = null;

	private _fd: number;
	private _connectionCount: number;

	private constructor(fd: number) {
		this._fd = fd;
		this._connectionCount = 0;
	}

	public listen(cb: ConnectListener): void {
		const furl = new URL(import.meta.url);
		const workerScript = join(dirname(furl.pathname), 'ServerWorker.js');
		const worker = new Worker(workerScript, { workerData: this._fd });
		worker.on('message', async (fd: number) => {
			const sock = new Socket({ fd });

			if (this._connectionCount >= 128) {
				await nack(sock, 'Error: max connections');
				return;
			}

			try {
				await ack(sock);
			} catch (err) {
				console.error('Failed to send ack response: ', err.message);
				return;
			}

			this._connectionCount += 1;

			sock.on('close', () => {
				this._connectionCount -= 1;
			});

			cb(sock);
		});
	}

	public close(): Promise<void> {
		return new Promise<void>((res, rej) => {
			close(this._fd, (err) => {
				if (err) rej(err);
				else res();
			});
		});
	}

	public static create(): CatuiServer {
		if (CatuiServer._inst) {
			throw new Error('CatuiServer.listen() has already been called');
		}

		const fd = loadBalancerFd();
		const server = (CatuiServer._inst = new CatuiServer(fd));
		return server;
	}
}

function loadBalancerFd(): number {
	const fdVar = process.env.CATUI_LOAD_BALANCER_FD;
	if (!fdVar) throw new Error('CATUI_LOAD_BALANCER_FD not defined');

	const fd = parseInt(fdVar, 10);
	if (isNaN(fd) || fd < 0)
		throw new Error(
			`CATUI_LOAD_BALANCER_FD ("${fdVar}") is not a positive integer`,
		);

	return fd;
}

const ACK_BUFSIZE = 1024;
const emptyBuf = new Uint8Array(0);

async function ack(sock: Socket): Promise<void> {
	const msg = new MsgStreamSocket(sock);
	await msg.send(emptyBuf, ACK_BUFSIZE);
}

async function nack(sock: Socket, errMsg: string): Promise<void> {
	const msg = new MsgStreamSocket(sock);
	const json = JSON.stringify({ error: errMsg });
	const buf = Buffer.from(json, 'utf8');
	await msg.send(buf, ACK_BUFSIZE);
}
