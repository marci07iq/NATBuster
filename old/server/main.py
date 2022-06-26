
import asyncio
import websockets

import json

async def handler(websocket):
    while True:
        try:
            message = await websocket.recv()

            decoded = json.loads(message)

        except simplejson.decoder.JSONDecodeError:
            print('Decoding JSON has failed')
            break
        except websockets.ConnectionClosedOK:
            break
        print(message)


async def main():

    async with websockets.serve(handler, "", 8001):
        await asyncio.Future()  # run forever

if __name__ == "__main__":
    asyncio.run(main())
