window.addEventListener('load', () => {
	const input = document.getElementById('bg-color-input');
	const style = getComputedStyle(document.body);
	input.value = style.backgroundColor;
	const submit = document.getElementById('submit');
	submit.click();
});
