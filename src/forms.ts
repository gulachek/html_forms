import { EventEmitter, Handler, EventOf } from './events';

type HtmlFormsEvents = {
	message: [string];
};

const events = new EventEmitter<HtmlFormsEvents>();

export function on<TEvent extends EventOf<HtmlFormsEvents> & string>(
	event: TEvent,
	handler: Handler<HtmlFormsEvents, TEvent>,
): void {
	events.on(event, handler);
}

export function off<TEvent extends EventOf<HtmlFormsEvents> & string>(
	event: TEvent,
	handler: Handler<HtmlFormsEvents, TEvent>,
): void {
	events.off(event, handler);
}

window.addEventListener('submit', (e) => {
	const form = e.target as HTMLFormElement;
	if (!form) return;
	if (form.hasAttribute('action')) return;

	const method = form.getAttribute('method');
	if (method && method !== 'POST') return;

	// Form w/o action and default (POST) form. Submit data
	e.preventDefault(); // don't actually submit
	const data = new FormData(form, e.submitter);
	const params = new URLSearchParams(data as any);

	const xhr = new XMLHttpRequest();
	xhr.open('POST', '~/submit');
	xhr.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded');
	xhr.send(params.toString());
});

interface IShowAlertOpts {
	title?: string;
}

// :)
interface SendJsMessage {
	type: 'send';
	msg: string;
}

// application output to user
type OutputMessage = SendJsMessage;

interface RecvJsMessage {
	type: 'recv';
	msg: string;
}

// user input
type InputMessage = RecvJsMessage;

class Connection {
	private ws: WebSocket;
	private dialog: HTMLDialogElement;
	private isOpen: Promise<void>;
	private resolveOpen?: () => void;
	private rejectOpen?: () => void;

	constructor() {}

	start() {
		this.isOpen = new Promise((res, rej) => {
			this.resolveOpen = res;
			this.rejectOpen = rej;
		});

		const url = new URL('~/ws', document.baseURI);
		url.protocol = 'ws';
		this.ws = new WebSocket(url);

		// Event listener for when the connection is opened
		this.ws.addEventListener('open', (e) => {
			this.onOpen(e);
		});

		// Event listener for receiving messages from the server
		this.ws.addEventListener('message', (e) => {
			this.onMessage(e);
		});

		// Event listener for handling errors
		this.ws.addEventListener('error', (e) => {
			this.onError(e);
		});

		// Event listener for when the connection is closed
		this.ws.addEventListener('close', (e) => {
			this.onClose(e);
		});
	}

	showAlert(msg: string, opts?: IShowAlertOpts) {
		if (this.dialog) {
			document.body.removeChild(this.dialog);
		}

		opts = opts || {};

		this.dialog = document.createElement('dialog');

		if (opts.title) {
			const h1 = document.createElement('h1');
			h1.innerText = opts.title;
			this.dialog.appendChild(h1);
		}

		const p = document.createElement('p');
		p.innerText = msg;
		this.dialog.appendChild(p);

		document.body.appendChild(this.dialog);
		this.dialog.showModal();
	}

	async send(msg: InputMessage): Promise<void> {
		await this.isOpen;
		this.ws.send(JSON.stringify(msg));
	}

	onOpen(_e: Event) {
		this.resolveOpen();
		delete this.resolveOpen;
		delete this.rejectOpen;
	}

	onError(e: Event) {
		if (this.rejectOpen) {
			this.rejectOpen();
			delete this.resolveOpen;
			delete this.rejectOpen;
		}

		console.error(e);
		document.title = 'ERROR';
		this.showAlert('Connection error. See console for more details.', {
			title: 'Error',
		});
	}

	onClose(_e: Event) {
		document.title = 'DISCONNECTED';
		this.showAlert('The connection was broken. Please close the web page.', {
			title: 'Disconnected',
		});
	}

	onMessage(e: MessageEvent) {
		let obj: OutputMessage;
		try {
			obj = JSON.parse(e.data);
		} catch (ex) {
			alert('Error parsing JSON message from server. See console for details.');
			console.error(ex);
			return;
		}

		const type = obj.type;

		switch (type) {
			case 'send':
				const received = events.emit('message', obj.msg);
				if (!received) {
					this.showAlert(
						'The application sent a message but had not yet listened to messages in the browser',
						{ title: 'Warning' },
					);
				}
				break;
			default:
				this.showAlert(`Unknown action type: ${type}`);
		}
	}
}

const con = new Connection();
con.start();

export function sendMessage(msg: string): Promise<void> {
	return con.send({ type: 'recv', msg });
}
