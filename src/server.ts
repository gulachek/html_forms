import { Socket } from 'node:net';
import { readn } from './AsyncReadStream.js';
import { CatuiServer } from './CatuiServer.js';
import { send, recv } from './MsgStream.js';
import {
	createServer,
	IncomingMessage,
	Server,
	ServerResponse,
} from 'node:http';
import { randomUUID } from 'node:crypto';
import { Readable, Writable } from 'node:stream';
import { pipeline } from 'node:stream/promises';
import EventEmitter from 'node:events';
import { exec } from 'node:child_process';

const MSG_SIZE = 2048;
const PROMPT_SIZE = 4096;

enum MsgType {
	beginUpload = 0,
	promptUser = 1,
}

class BufferStream extends Writable {
	private _chunks: Buffer[] = [];
	private _len: number = 0;

	constructor() {
		super();
	}

	override _writev(chunks: any, callback: (err: Error) => void) {
		for (const { chunk } of chunks) {
			if (chunk.length + this._len > 8192) {
				callback(new Error('Writing chunk would exceed maximum buffer length'));
				return;
			}

			this._chunks.push(chunk);
			this._len += chunk.length;
		}

		callback(null);
	}

	consume() {
		const out = Buffer.concat(this._chunks);
		this._chunks = [];
		this._len = 0;
		return out;
	}
}

function normalizeDirectories(pieces: string[]): string[] {
	const normalized: string[] = [];
	for (const piece of pieces) {
		if (!piece || piece === '.') continue;

		if (piece === '..') {
			normalized.pop();
		} else {
			normalized.push(piece);
		}
	}

	return normalized;
}

function normalizeVirtualPath(pieces: string[]): string[] {
	let normalized: string[] = [];
	for (const piece of pieces) {
		if (piece === '~') {
			normalized = [];
		} else {
			normalized.push(piece);
		}
	}

	return normalized;
}

interface IUploadMessage {
	type: MsgType.beginUpload;
	url: string;
	mime: string;
	size: number;
}

interface IPromptMessage {
	type: MsgType.promptUser;
	url: string;
}

interface IFile {
	url: string;
	mime: string;
	content: Uint8Array;
}

function isUpload(obj: unknown): obj is IUploadMessage {
	if (typeof obj !== 'object') return false;
	if (!('type' in obj && obj.type === MsgType.beginUpload)) return false;

	if (!('url' in obj && typeof obj.url === 'string')) return false;
	if (!('mime' in obj && typeof obj.mime === 'string')) return false;
	if (!('size' in obj && typeof obj.size === 'number')) return false;

	return true;
}

function isPrompt(obj: unknown): obj is IPromptMessage {
	if (typeof obj !== 'object') return false;
	if (!('type' in obj && obj.type === MsgType.promptUser)) return false;

	if (!('url' in obj && typeof obj.url === 'string')) return false;

	return true;
}

class HtmlSession {
	private _sock: Socket;
	private _files = new Map<string, IFile>();
	private _uuid: string;
	private _baseUrl: string;
	private _events = new EventEmitter();

	constructor(sock: Socket, baseUrl: string, uuid: string) {
		this._sock = sock;
		this._baseUrl = baseUrl;
		this._uuid = uuid;
	}

	async start(): Promise<void> {
		let connected = true;

		this._sock.on('close', () => {
			connected = false;
		});

		while (connected) {
			const msg = await recv(this._sock, MSG_SIZE);
			if (msg === null) {
				connected = false;
				break;
			}

			const decoder = new TextDecoder();
			const json = decoder.decode(msg);
			const obj = JSON.parse(json);
			if (isUpload(obj)) {
				await this.uploadFile(obj);
			} else if (isPrompt(obj)) {
				this.promptUser(obj);
			}
		}

		this._events.emit('close');
	}

	public onClose(cb: () => void): void {
		this._events.on('close', cb);
	}

	public findFile(url: string): IFile | null {
		const file = this._files.get(url);
		if (!file) return null;
		return file;
	}

	public async submitForm(stream: Readable): Promise<void> {
		const bufStream = new BufferStream();
		await pipeline(stream, bufStream);
		await send(this._sock, bufStream.consume(), PROMPT_SIZE);
	}

	private async uploadFile(obj: IUploadMessage) {
		console.log('Uploading file', obj.url);
		const content = await readn(this._sock, obj.size);
		this._files.set(obj.url, {
			url: obj.url,
			mime: obj.mime,
			content,
		});
	}

	private promptUser(obj: IPromptMessage) {
		const url = `${this._baseUrl}${this._uuid}${obj.url}`;
		console.log(`Prompting: ${url}`);
		exec(`open '${url}'`);
	}
}

class WebServer {
	private _http: Server;
	private _port: number;
	private _sessions = new Map<string, HtmlSession>();

	constructor(port: number) {
		this._port = port;
		this._http = createServer((req, res) => {
			this.serveRequest(req, res);
		});
	}

	public listen(): void {
		this._http.listen(this._port);
	}

	private async serveRequest(
		req: IncomingMessage,
		res: ServerResponse<IncomingMessage>,
	): Promise<void> {
		const baseUrl = `http://localhost:${this._port}`;
		const url = new URL(req.url, baseUrl);

		const headers: Record<string, string> = {
			'Cache-Control': 'no-store',
		};

		console.log(req.method, url.pathname);

		const rawPieces = url.pathname.split('/');

		let pieces = normalizeDirectories(rawPieces);

		if (pieces.length < 1) {
			res.writeHead(404, 'Not Found', headers);
			res.end('No session specified');
			return;
		}

		const uuid = pieces[0];
		pieces = normalizeVirtualPath(pieces.slice(1));

		console.log('looking up session', uuid);
		const session = this._sessions.get(uuid);
		if (!session) {
			res.writeHead(404, 'Not Found', headers);
			res.end('Session not found');
			return;
		}

		if (req.method === 'GET') {
			if (url.pathname.endsWith('/')) {
				rawPieces.push('index.html');
			}

			const normalizedUrl = '/' + pieces.join('/');
			console.log('looking up file', normalizedUrl);

			const file = session.findFile(normalizedUrl);
			if (!file) {
				res.writeHead(404, 'Not Found', headers);
				res.end('File not found');
				return;
			}

			headers['Content-Type'] = file.mime;
			res.writeHead(200, 'Ok', headers);
			res.end(file.content);
		} else if (req.method === 'POST') {
			if (pieces.length === 1 && pieces[0] === 'submit') {
				await session.submitForm(req);
				res.writeHead(200, 'ok', headers);
				res.end('ok');
				return;
			}
		} else {
			res.writeHead(400, 'Bad request', headers);
			res.end('Bad request method');
			return;
		}
	}

	public addSession(uuid: string, session: HtmlSession): void {
		this._sessions.set(uuid, session);
		session.onClose(() => {
			this._sessions.delete(uuid);
		});
	}
}

const port = 9999;
const baseUrl = `http://localhost:${port}/`;
const webServer = new WebServer(port);

webServer.listen();

const server = CatuiServer.create();
server.listen((sock) => {
	const uuid = randomUUID();
	const session = new HtmlSession(sock, baseUrl, uuid);
	webServer.addSession(uuid, session);
	session.start();
});
