# Websocket transport layer

import asyncio
#from websockets import connect

class TransportWebsocket:
	def __init__(self, ws):
		self.ws = ws

	def send(self, packet):
		self.ws.send(packet)

	#Infinte async iterable
	async def receive(self):
		async for message in websocket:
        	yield message