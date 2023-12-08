export type EventOf<TEvents> = keyof TEvents;
export type HandlerArgs<
	TEvents,
	TEvent extends EventOf<TEvents>,
> = TEvents[TEvent] & unknown[];

export type Handler<TEvents, TEvent extends EventOf<TEvents>> = (
	...args: HandlerArgs<TEvents, TEvent>
) => void;

export class EventEmitter<TEvents> {
	listeners = new Map<EventOf<TEvents>, Set<Function>>();

	on<TEvent extends EventOf<TEvents> & string>(
		event: TEvent,
		handler: Handler<TEvents, TEvent>,
	): void {
		let set = this.listeners.get(event);
		if (!set) {
			set = new Set<Function>();
			this.listeners.set(event, set);
		}

		set.add(handler);
	}

	off<TEvent extends EventOf<TEvents> & string>(
		event: TEvent,
		handler: Handler<TEvents, TEvent>,
	): void {
		const set = this.listeners.get(event);
		if (!set) return;
		set.delete(handler);
	}

	emit<TEvent extends EventOf<TEvents> & string>(
		event: TEvent,
		...args: HandlerArgs<TEvents, TEvent>
	): boolean {
		const set = this.listeners.get(event);
		if (!set) return false;

		for (const handler of set) {
			handler(...args);
		}

		return set.size > 0;
	}
}
