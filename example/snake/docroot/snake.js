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

let lastMsg;
const GAME_WIDTH = 40;
const GAME_HEIGHT = 30;

function resizeCanvas(canvas) {
	const { clientWidth, clientHeight } = document.documentElement;
	const pxPerUnitX = Math.max(Math.floor(clientWidth / GAME_WIDTH), 3);
	const pxPerUnitY = Math.max(Math.floor(clientHeight / GAME_HEIGHT), 3);
	const pxPerUnit = Math.min(pxPerUnitX, pxPerUnitY);

	canvas.width = pxPerUnit * GAME_WIDTH;
	canvas.height = pxPerUnit * GAME_HEIGHT;
}

function render(canvas, ctx) {
	if (!lastMsg) return;

	const { width, height } = canvas;

	const unitX = width / GAME_WIDTH;
	const unitY = height / GAME_HEIGHT;

	// clear
	ctx.fillStyle = 'rgb(255, 255, 255)';
	ctx.fillRect(0, 0, width, height);

	// draw snake
	ctx.fillStyle = 'rgb(0, 255, 0)';
	for (const [gx, gy] of lastMsg.snake) {
		const x = gx * unitX;
		const y = gy * unitY;
		ctx.fillRect(x, y, unitX, unitY);
	}

	// draw fruit
	const [sx, sy] = lastMsg.fruit;
	ctx.fillStyle = 'rgb(255, 200, 0)';
	ctx.fillRect(sx * unitX, sy * unitY, unitX, unitY);
}

async function main() {
	const canvas = document.getElementById('canvas');
	const ctx = canvas.getContext('2d');
	resizeCanvas(canvas);

	HtmlForms.on('message', (msg) => {
		try {
			lastMsg = JSON.parse(msg);
		} catch (e) {
			console.error(e);
			alert('Error parsing JSON message. See console for details.');
		}

		render(canvas, ctx);
	});

	window.addEventListener('keydown', (e) => {
		onKeyDown(e.key);
	});

	window.addEventListener('resize', () => {
		resizeCanvas(canvas);
		render(canvas, ctx);
	});

	await HtmlForms.sendMessage('<sync>');
	started = true;
}

window.addEventListener('load', () => {
	main();
});
