import { app, BrowserWindow } from 'electron';

app.whenReady().then(() => {
	const win = new BrowserWindow();
	win.loadURL('https://google.com');
});
