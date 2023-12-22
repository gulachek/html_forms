const keyMap = {
	h: 'left',
	j: 'down',
	k: 'up',
	l: 'right',
	ArrowUp: 'up',
	ArrowDown: 'down',
	ArrowLeft: 'left',
	ArrowRight: 'right',
};

let started = false;

function onKeyDown(key) {
	if (!started) return;

	const dir = keyMap[key];
	if (!dir) return;

	HtmlForms.sendMessage(dir);
}

function onMessage(canvas, ctx, msg) {
	const { width, height } = canvas;
	const gameWidth = 40;
	const gameHeight = 30;

	const unitX = width / gameWidth;
	const unitY = height / gameHeight;

	let obj;
	try {
		obj = JSON.parse(msg);
	} catch (e) {
		console.error(e);
		alert('Error parsing JSON message. See console for details.');
	}

	// clear
	ctx.fillStyle = 'rgb(255, 255, 255)';
	ctx.fillRect(0, 0, width, height);

	// draw snake
	ctx.fillStyle = 'rgb(0, 255, 0)';
	for (const [gx, gy] of obj.snake) {
		const x = gx * unitX;
		const y = gy * unitY;
		ctx.fillRect(x, y, unitX, unitY);
	}

	// draw fruit
	const [sx, sy] = obj.fruit;
	ctx.fillStyle = 'rgb(255, 200, 0)';
	ctx.fillRect(sx * unitX, sy * unitY, unitX, unitY);
}

async function main() {
	const canvas = document.getElementById('canvas');
	const ctx = canvas.getContext('2d');

	HtmlForms.on('message', (msg) => {
		onMessage(canvas, ctx, msg);
	});

	window.addEventListener('keydown', (e) => {
		onKeyDown(e.key);
	});

	await HtmlForms.sendMessage('<sync>');
	started = true;
}

window.addEventListener('load', () => {
	main();
});
