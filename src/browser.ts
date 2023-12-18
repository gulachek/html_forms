import { app, BrowserWindow } from 'electron';
import { recv, send } from './MsgStream';

const BUF_SIZE = 2048;

const windows = new Map<number, BrowserWindow>();

interface OpenMessage {
	type: 'open';
	url: string;
	windowId: number;
}

interface CloseMessage {
	type: 'close';
	windowId: number;
}

type OutputMessage = OpenMessage | CloseMessage;
type InputMessage = CloseMessage;

async function sendMsg(msg: InputMessage) {
	const buf = Buffer.from(JSON.stringify(msg));
	await send(process.stdout, buf, BUF_SIZE);
}

// work w/ async io
process.stdin.pause();

app.whenReady().then(async () => {
	const decoder = new TextDecoder();

	while (true) {
		const msg = await recv(process.stdin, BUF_SIZE);
		if (!msg) break;

		const jobj = JSON.parse(decoder.decode(msg)) as OutputMessage;

		if (jobj.type === 'open') {
			const { windowId, url } = jobj;
			let win = windows.get(windowId);

			if (!win) {
				win = new BrowserWindow();
				windows.set(windowId, win);

				win.on('close', (e) => {
					e.preventDefault(); // let app do closing
					sendMsg({ type: 'close', windowId });
				});

				win.webContents.on('before-input-event', (e, input) => {
					if (input.type !== 'keyDown') return;
					if (
						input.key !== 'F12' ||
						input.alt ||
						input.control ||
						input.shift ||
						input.meta ||
						input.isAutoRepeat
					)
						return;

					e.preventDefault();
					if (win.webContents.isDevToolsOpened()) {
						win.webContents.closeDevTools();
					} else {
						win.webContents.openDevTools();
					}
				});
			}

			win.loadURL(url);
		} else if (jobj.type === 'close') {
			const { windowId } = jobj;
			const win = windows.get(windowId);
			if (!win) return;
			win.destroy();
			windows.delete(windowId);
		}
	}
});
