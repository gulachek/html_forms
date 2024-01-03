window.addEventListener('submit', (e) => {
	const form = e.target as HTMLFormElement;
	if (!form) return;
	if (!form.hasAttribute('action')) {
		form.action = '~/submit';
	}

	if (!form.hasAttribute('method')) {
		form.method = 'POST';
	}
});

export function connect(): WebSocket {
	const url = new URL('~/ws', document.baseURI);
	url.protocol = 'ws';
	return new WebSocket(url);
}
