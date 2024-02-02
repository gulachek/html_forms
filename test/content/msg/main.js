window.addEventListener('load', () => {
	const ws = HtmlForms.connect();

	ws.addEventListener('open', () => {
		ws.send('start!');
	});

	ws.addEventListener('error', (e) => {
		console.error(e);
		alert('An error occurred. See console for details');
	});

	ws.addEventListener('close', () => {
		alert('Disconnected. Please close the window.');
	});

	ws.addEventListener('message', (e) => {
		try {
			ws.send(e.data); // echo
		} catch (er) {
			console.error(er);
			alert('Error parsing JSON message. See console for details.');
		}
	});
});
