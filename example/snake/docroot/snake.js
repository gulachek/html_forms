let ws;
let fps;

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
	if (!started) return false;

	const dir = keyMap[key];
	if (!dir) return false;

	ws.send(dir);
	return true;
}

let lastMsg;
const GAME_WIDTH = 40;
const GAME_HEIGHT = 30;
const FPS_NFRAMES = 10;

class FrameRate {
	times;
	index;
	elem;
	fmt;

	constructor(elem) {
		this.elem = elem;
		this.times = [];
		this.index = 0;
		for (let i = 0; i < FPS_NFRAMES; ++i) {
			this.times.push(performance.now());
		}

		this.fmt = new Intl.NumberFormat('en-US', {
			maximumFractionDigits: 2,
			minimumFractionDigits: 2,
			minimumIntegerDigits: 2,
		});
	}

	update() {
		const now = performance.now();
		this.times[this.index] = now;
		this.index = (this.index + 1) % this.times.length;

		// render on each cycle
		if (this.index == 0) {
			this.elem.innerText = this.fps();
		}
	}

	fps() {
		let sumDiffsMs = 0;
		const diffCount = this.times.length - 1;
		for (let i = 0; i < diffCount; ++i)
			sumDiffsMs += this.times[i + 1] - this.times[i];

		const fpsStr = this.fmt.format((diffCount * 1000) / sumDiffsMs);
		return `${fpsStr} fps`;
	}
}

function resizeCanvas(canvas) {
	const { clientWidth, clientHeight } = canvas.parentElement;
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

	// compute fps
	fps.update();
}

async function main() {
	const canvas = document.getElementById('canvas');
	const ctx = canvas.getContext('2d');
	resizeCanvas(canvas);

	fps = new FrameRate(document.getElementById('frame-rate'));

	ws = HtmlForms.connect();
	ws.binaryType = 'arraybuffer';

	ws.addEventListener('open', () => {
		ws.send('<sync>');
		started = true;
	});

	ws.addEventListener('error', (e) => {
		console.error(e);
		alert('An error occurred. See console for details');
	});

	ws.addEventListener('close', () => {
		alert('Disconnected. Please close the window.');
	});

	const decoder = new TextDecoder();

	ws.addEventListener('message', (e) => {
		try {
			// data is binary
			const json = decoder.decode(e.data);
			lastMsg = JSON.parse(json);
		} catch (er) {
			console.error(er);
			alert('Error parsing JSON message. See console for details.');
		}

		render(canvas, ctx);
	});

	window.addEventListener('keydown', (e) => {
		if (onKeyDown(e.key)) {
			e.preventDefault();
		}
	});

	window.addEventListener('resize', () => {
		resizeCanvas(canvas);
		render(canvas, ctx);
	});
}

window.addEventListener('load', () => {
	main();
});
