import { app, BrowserWindow, dialog, screen } from 'electron';
import { recv, send } from './MsgStream';

const BUF_SIZE = 2048;

const windows = new Map<string, BrowserWindow>();

interface OpenMessage {
	type: 'open';
	url: string;
	sessionId: string;
}

interface CloseMessage {
	type: 'close';
	sessionId: string;
}

interface ErrorMessage {
	type: 'error';
	sessionId: string;
	msg: string;
}

type OutputMessage = OpenMessage | CloseMessage | ErrorMessage;
type InputMessage = CloseMessage;

async function sendMsg(msg: InputMessage) {
	const buf = Buffer.from(JSON.stringify(msg));
	await send(process.stdout, buf, BUF_SIZE);
}

function makeWindow(sessionId: string): BrowserWindow {
	const win = new BrowserWindow({ show: false });

	const centerAndShowWindow = () => {
		const { width, height, x, y } = win.getContentBounds();
		const mid = {
			x: Math.floor(x + width / 2),
			y: Math.floor(y + height / 2),
		};
		const display = screen.getDisplayNearestPoint(mid);
		const sz = display.workAreaSize;
		const margin = 64;
		win.setSize(
			Math.max(sz.width - 2 * margin, 800),
			Math.max(sz.height - 2 * margin, 800),
		);
		win.center();
		win.show();
		win.off('ready-to-show', centerAndShowWindow);
	};

	win.on('ready-to-show', centerAndShowWindow);
	win.on('close', (e) => {
		if (!windows.has(sessionId)) return;
		e.preventDefault(); // let app do closing
		sendMsg({ type: 'close', sessionId });
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

	return win;
}

// work w/ async io
process.stdin.pause();

app.whenReady().then(async () => {
	const decoder = new TextDecoder();

	while (true) {
		const msg = await recv(process.stdin, BUF_SIZE);
		if (!msg) {
			// Panic
			for (const [_, win] of windows.entries()) {
				win.destroy();
			}
			windows.clear();
			break;
		}

		const jobj = JSON.parse(decoder.decode(msg)) as OutputMessage;

		if (jobj.type === 'open') {
			const { sessionId, url } = jobj;
			let win = windows.get(sessionId);

			if (!win) {
				win = makeWindow(sessionId);
				windows.set(sessionId, win);
			}

			win.loadURL(url);
		} else if (jobj.type === 'close') {
			const { sessionId } = jobj;
			const win = windows.get(sessionId);
			if (!win) return;
			win.destroy();
			windows.delete(sessionId);
		} else if (jobj.type === 'error') {
			const { sessionId, msg } = jobj;
			const win = windows.get(sessionId);
			if (!win) return;
			dialog.showMessageBox(win, {
				type: 'error',
				message: msg,
			});
			windows.delete(sessionId);
		}
	}
});
