import { Readable } from 'node:stream';

export async function readn(
	stream: Readable,
	n: number,
): Promise<Uint8Array | null> {
	while (stream.readableLength < n && !stream.closed) {
		await waitReadable(stream);
	}

	const chunk = stream.read(n);
	if (chunk.length === n) {
		return chunk as Uint8Array;
	} else if (chunk.length === 0) {
		return null;
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
		console.log('readable event');
		resolve();
		stream.off('readable', onReadable);
	};

	stream.on('readable', onReadable);
	return promise;
}
