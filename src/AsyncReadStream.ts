import { Readable } from 'node:stream';

export async function readn(
	stream: Readable,
	n: number,
): Promise<Uint8Array | null> {
	while (stream.readableLength < n && !stream.closed) {
		await waitReadable(stream);
	}

	const chunk = stream.read(n);
	if (!chunk || chunk.length === 0) {
		return null;
	} else if (chunk.length === n) {
		return chunk as Uint8Array;
	} else {
		throw new Error(
			`Expected to read ${n} bytes but only read ${chunk.length}`,
		);
	}
}

function waitReadable(stream: Readable): Promise<void> {
	let resolve: () => void;
	const promise = new Promise<void>((res) => {
		resolve = res;
	});

	const onReadable = () => {
		resolve();
		stream.off('readable', onReadable);
	};

	stream.on('readable', onReadable);
	return promise;
}
