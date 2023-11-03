import { Socket } from 'node:net';

export function getFd(sock: Socket): number {
	return (sock as any)._handle.fd;
}
