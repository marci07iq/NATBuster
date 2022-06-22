import OpenSSL

import asyncio
import websockets

import json

from enum import Enum


class AuthFlowPKIState(Enum):
  #Request created by source
  #Establish servers identity by sending a nonce
  INITIATED = 1,
  #Destination acknowledges auth attempt
  #Sends 
  CHALLENGE = 2,
  


class AuthFlowPKI:
  def __init__(self, source, dest):
    self.client_src = source_client
    self.client_dst = dest

    self.state = AuthFlowPKIState.

  def recv(message):
    pass

class ClientConnection:
  def __init__(self, ws):
    self.ws = ws
  def 


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