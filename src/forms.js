window.addEventListener('submit', (e) => {
	const form = e.target;
	if (!form) return;
	if (form.hasAttribute('action')) return;

	const method = form.getAttribute('method');
	if (method && method !== 'POST') return;

	// Form w/o action and default (POST) form. Submit data
	e.preventDefault(); // don't actually submit
	const data = new FormData(form, e.submitter);
	const params = new URLSearchParams(data);

	const xhr = new XMLHttpRequest();
	xhr.open('POST', '~/submit');
	xhr.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded');
	xhr.send(params.toString());
});

class Connection {
	ws;
	dialog;

	constructor() {}

	start() {
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

	showAlert(msg, opts) {
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

	onOpen(_e) {
		//this.ws.send('Hello, Server!');
	}

	onError(e) {
		console.error(e);
		document.title = 'ERROR';
		this.showAlert('Connection error. See console for more details.', {
			title: 'Error',
		});
	}

	onClose(_e) {
		document.title = 'DISCONNECTED';
		this.showAlert('The connection was broken. Please close the web page.', {
			title: 'Disconnected',
		});
	}

	onMessage(e) {
		let obj;
		try {
			obj = JSON.parse(e.data);
		} catch (ex) {
			alert('Error parsing JSON message from server. See console for details.');
			console.error(ex);
		}

		switch (obj.type) {
			case 'navigate':
				window.location.href = obj.href;
				break;
			default:
				alert(`Unknown action type: ${obj.type}`);
		}
	}
}

const con = new Connection();
con.start();
