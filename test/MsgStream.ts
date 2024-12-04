import { Readable, Writable } from 'node:stream';
import { readn } from './AsyncReadStream';

export async function send(
	stream: Writable,
	buf: Uint8Array,
	bufSize: number,
): Promise<void> {
	if (bufSize <= 0)
		throw new Error(`Buf size (${bufSize}) must be greater than zero`);

	await writeHeader(stream, bufSize, buf.length);
	if (buf.length > 0) {
		await write(stream, buf);
	}
}

export async function recv(
	stream: Readable,
	bufSize: number,
): Promise<Uint8Array | null> {
	if (!stream.isPaused()) {
		throw new Error(`msgstream recv must operate on a paused stream`);
	}

	if (bufSize <= 0)
		throw new Error(`Buf size (${bufSize}) must be greater than zero`);

	const msgSize = await readHeader(stream, bufSize);
	if (msgSize === null) return null;

	if (msgSize <= 0) return emptyBuffer;

	const msg = await readn(stream, msgSize);
	return msg;
}

async function readHeader(
	stream: Readable,
	bufSize: number,
): Promise<number | null> {
	const nHeader = headerSize(bufSize);
	if (nHeader > 0xff)
		throw new Error(
			`msgstream header size of '${nHeader}' is too big. must fit in one byte`,
		);

	const firstByte = await readn(stream, 1);
	if (!firstByte) {
		return null;
	}

	if (firstByte[0] !== nHeader)
		throw new Error(
			`Received unexpected msgstream header size. Expected '${nHeader}' but received '${firstByte[0]}'`,
		);

	const header = await readn(stream, nHeader - 1);
	let msgSize = 0;
	let mult = 1;
	for (let i = 0; i < header.length; ++i) {
		msgSize += mult * header[i];
		mult *= 256;
	}

	return msgSize;
}

const emptyBuffer = new Uint8Array(0);

function write(stream: Writable, buf: Uint8Array): Promise<void> {
	return new Promise<void>((res, rej) => {
		stream.write(buf, (err) => {
			if (err) rej(err);
			else res();
		});
	});
}

async function writeHeader(
	stream: Writable,
	bufSize: number,
	msgSize: number,
): Promise<void> {
	if (msgSize > bufSize)
		throw new Error(
			`Buffer size '${bufSize}' is not large enough to fit message of size '${msgSize}'`,
		);

	const nheader = headerSize(bufSize);
	const buf = new Uint8Array(nheader);
	if (nheader > 0xff) {
		throw new Error(
			`msgstream header size of '${nheader}' is too big. must fit in one byte`,
		);
	}

	buf[0] = nheader;

	let i = 1;
	while (msgSize > 0) {
		const nextSize = Math.floor(msgSize / 256);
		buf[i] = msgSize - nextSize;
		msgSize = nextSize;
		i += 1;
	}

	return write(stream, buf);
}

function headerSize(bufSize: number): number {
	let nbytes = 0;
	while (bufSize > 0) {
		bufSize = Math.floor(bufSize / 256);
		nbytes += 1;
	}

	return 1 + nbytes;
}
