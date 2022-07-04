## List of features to add:
- Common:
  - ECDHE wrapper
  - KEX and encryption system
- C2 Server:
  - Everything.
- IP Server:
  - Everything.
- Client:
  - Everything.

## List of changes to already existing code:
- RAII wrap all OpenSSL contexts, even when used inside a single function
- Create a new class for handling binary data blobs
  - Allocating more capacity then size (like std::vector)
  - Allocate with a gap before the data (for easy adding envelopes)

