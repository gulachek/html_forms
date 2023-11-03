import { Readable } from 'node:stream';

export class AsyncReadStream {
	private _stream: Readable;
	private _chunks: Buffer[] = [];

	// total amount of data in chunks
	private _totalLen: number = 0;

	// offset into first chunk
	private _offset: number = 0;

	private _requestedLength: number = 0;

	private _resolve: null | ((buf: Uint8Array | null) => void) = null;
	private _reject: null | ((err: Error) => void) = null;

	constructor(stream: Readable) {
		this._stream = stream;
		this._stream.on('data', (chunk) => this.onData(chunk));
		this._stream.on('close', () => this.onClose());
	}

	public get length(): number {
		return this._totalLen - this._offset;
	}

	private onClose(): void {
		if (this._reject && this._resolve) {
			if (this.length === 0) {
				this._resolve(null);
			} else {
				this._reject(new Error('EOF'));
			}

			this._reject = null;
			this._resolve = null;
		}
	}

	private onData(chunk: Buffer): void {
		this._chunks.push(chunk);
		this._totalLen += chunk.length;
		this._pump();
	}

	private _pump(): void {
		if (!this._resolve) return;
		if (this.length < this._requestedLength) return;

		this._resolve(this._consume(this._requestedLength));
		this._requestedLength = 0;
		this._resolve = null;
		this._reject = null;
	}

	private _consume(n: number): Uint8Array {
		if (n > this.length) throw new Error('out of bounds');

		const buf = new Uint8Array(n);
		for (let i = 0; i < n; ++i) {
			const chunk = this._chunks[0];
			buf[i] = chunk[this._offset++];

			if (this._offset >= chunk.length) {
				this._chunks.shift();
				this._offset = 0;
				this._totalLen -= chunk.length;
			}
		}

		return buf;
	}

	public async readn(n: number): Promise<Uint8Array | null> {
		if (this._resolve) throw new Error('Read is already in progress');
		if (this._stream.destroyed) throw new Error('Stream is closed');

		this._requestedLength = n;
		const prom = new Promise<Uint8Array>((res, rej) => {
			this._resolve = res;
			this._reject = rej;
		});

		this._pump();
		return prom;
	}
}
