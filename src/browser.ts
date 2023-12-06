import { app, BrowserWindow } from 'electron';
import { recv } from './MsgStream';

const BUF_SIZE = 2048;

const windows = new Map<number, BrowserWindow>();

interface OpenMessage {
	type: 'open';
	url: string;
	windowId: number;
}

type OutputMessage = OpenMessage;

// work w/ async io
process.stdin.pause();

app.whenReady().then(async () => {
	const decoder = new TextDecoder();

	const main = new BrowserWindow();
	main.loadURL('https://gulachek.com');

	while (true) {
		const msg = await recv(process.stdin, BUF_SIZE);
		if (!msg) break;

		const jobj = JSON.parse(decoder.decode(msg)) as OutputMessage;

		if (jobj.type === 'open') {
			const { windowId, url } = jobj;
			const win = new BrowserWindow();
			windows.set(windowId, win);
			win.loadURL(url);
			win.title = 'Testing!';
		}
	}
});
