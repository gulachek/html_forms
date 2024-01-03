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

export function connect(): WebSocket {
	const url = new URL('~/ws', document.baseURI);
	url.protocol = 'ws';
	return new WebSocket(url);
}
