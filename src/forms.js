window.addEventListener('submit', (e) => {
	const form = e.target;
	if (!form) return;
	if (form.hasAttribute('action')) return;

	const method = form.getAttribute('method');
	if (method && method !== 'POST') return;

	// Form w/o action and default (POST) form. Submit data
	e.preventDefault(); // don't actually submit
	const data = new FormData(form);
	const params = new URLSearchParams(data);

	const xhr = new XMLHttpRequest();
	xhr.open('POST', '~/submit');
	xhr.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded');
	xhr.send(params.toString());
});

class Connection {
	ws;

	constructor() {}

	start() {
		const url = new URL('~/ws', document.baseURI);
		url.protocol = 'ws';
		this.ws = new WebSocket(url);

		// Event listener for when the connection is opened
		this.ws.addEventListener('open', () => {
			console.log('WebSocket connection opened');

			// Send a message to the server after the connection is established
			this.ws.send('Hello, Server!');
		});

		// Event listener for receiving messages from the server
		this.ws.addEventListener('message', (e) => {
			this.onMessage(e);
		});

		// Event listener for handling errors
		this.ws.addEventListener('error', (e) => {
			console.error('WebSocket error:', e);
		});

		// Event listener for when the connection is closed
		this.ws.addEventListener('close', () => {
			console.log('WebSocket connection closed');
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
