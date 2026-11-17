import struct
import asyncio
import socket

MAGIC = bytes([2, 1, 4, 3, 6, 5, 8, 7])
SOCK = "/tmp/radar.sock"          # must match the SOCK used by the Socket server
FILE_CHUNK = 4096


class Client:
    def __init__(self, path = SOCK, chunk = FILE_CHUNK):
        self.path = path
        self.chunk = chunk
        self.sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self.sock.setblocking(False)

    async def start(self):
        await asyncio.get_running_loop().sock_connect(self.sock, self.path)

    async def receive(self):
        """Next chunk of bytes from the server, or b"" if the server closed."""
        return await asyncio.get_running_loop().sock_recv(self.sock, self.chunk)

    def close(self):
        self.sock.close()


def parse(data):
    """Returns (frames, rest). `rest` is an unfinished frame: prepend it to the next chunk."""
    frames = []
    pos = data.find(MAGIC)

    while pos >= 0:
        if pos + 40 > len(data):
            return frames, data[pos:]                # header not complete yet

        # header: magic, version, totalPacketLen, platform, frameNumber, timeCpuCycles, numDetectedObj, numTLVs, subFrameNumber
        _, _, total, _, frameNum, timeCpuCycles, _, num_tlvs, _ = struct.unpack_from("<8s8I", data, pos)
        if total < 40 or total % 32 or total > 64 * 1024:
            pos = data.find(MAGIC, pos + 1)          # false magic
            continue
        if pos + total > len(data):
            return frames, data[pos:]                # frame not complete yet: wait for more bytes

        points = []
        off = pos + 40
        for _ in range(num_tlvs):
            tlv_type, tlv_len = struct.unpack_from("<II", data, off)
            if tlv_type == 1:                        # detected points
                for i in range(tlv_len // 16):
                    points.append(struct.unpack_from("<4f", data, off + 8 + 16 * i))
            off += 8 + tlv_len

        frames.append({
            "frameNum": frameNum,
            "timeCpuCycles": timeCpuCycles,
            "points": points,
        })
        pos = data.find(MAGIC, pos + total)

    return frames, data[-(len(MAGIC) - 1):]          # no magic left: keep a possible partial magic


def calc_frame(frame):
    """Average of the absolute values of the points in a frame, or None if it has no points."""
    points = frame["points"]
    n = len(points)
    if n == 0:
        return None

    X, Y, Z, Doppler = 0, 0, 0, 0
    for x, y, z, doppler in points:
        X += abs(x)
        Y += abs(y)
        Z += abs(z)
        Doppler += abs(doppler)

    return X / n, Y / n, Z / n, Doppler / n


def calc_delta_speed(prev, cur):
    """Change between two frame averages (the tuples from calc_frame)."""
    return tuple(abs(c - p) for p, c in zip(prev, cur))


async def main(path = SOCK):
    client = Client(path)
    await client.start()
    buf = b""
    prev = None
    try:
        while data := await client.receive():
            frames, buf = parse(buf + data)
            for frame in frames:
                cur = calc_frame(frame)
                if cur is None:
                    continue                         # no points: keep the previous average
                if prev is not None:
                    dx, dy, dz, dDoppler = calc_delta_speed(prev, cur)
                    print(f"frame {frame['frameNum']}: dX = {dx:.2f}, dY = {dy:.2f}, "
                          f"dZ = {dz:.2f}, dDoppler = {dDoppler:.2f}")
                prev = cur
    finally:
        client.close()


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        pass
