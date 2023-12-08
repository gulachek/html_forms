HtmlForms.on('message', (msg) => {
	document.getElementById('response').innerText = msg;
});

HtmlForms.sendMessage('<ready>');
