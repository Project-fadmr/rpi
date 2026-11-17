import math
import struct
 
MAGIC = bytes([2, 1, 4, 3, 6, 5, 8, 7])



def parse(data) -> list(dict()):
    frames = []
    pos = data.find(MAGIC)


    while pos >= 0:
        frame = dict()
        # header: magic, version, totalPacketLen, platform, frameNumber, timeCpuCycles, numDetectedObj, numTLVs, subFrameNumber
        _, _, total, _, frameNum, timeCpuCycles, _, num_tlvs, _ = struct.unpack_from("<8s8I", data, pos)
        if total < 40 or pos + total > len(data):
            pos = data.find(MAGIC, pos + 1)      # bad or truncated frame
            continue


        points = []
        off = pos + 40
        for _ in range(num_tlvs):
            tlv_type, tlv_len = struct.unpack_from("<II", data, off)
            if tlv_type == 1:                    # detected points
                for i in range(tlv_len // 16):
                    points.append(struct.unpack_from("<4f", data, off + 8 + 16 * i))
            off += 8 + tlv_len

        frame={
            "frameNum": frameNum,
            "timeCpuCycles": timeCpuCycles,
            "points": points,
        }
    

        frames.append(frame)
        pos = data.find(MAGIC, pos + total)
    return frames


def calc_frame(data):
    data = data

    length = len(data.get("points"))


    X,Y,Z,Doppler = 0,0,0,0


    for i in data.get("points"):

        x, y, z, doppler = i
        X += abs(x)
        Y += abs(y)
        Z += abs(z)
        Doppler += abs(doppler)

    X /= length
    Y /= length
    Z /= length
    Doppler /= length

    return X,Y,Z,Doppler

def calc_delta_speed(data):
    X,Y,Z,Doppler = 0,0,0,0
    dx,dy,dz,dDoppler = 0,0,0,0

    for i, point in enumerate(data):
        x, y, z, doppler = point

        dx += math.sqrt(abs(math.pow(X,2) - math.pow(x,2)))
        dy += math.sqrt(abs(math.pow(Y,2) - math.pow(y,2)))
        dz += math.sqrt(abs(math.pow(Z,2) - math.pow(z,2)))
        dDoppler += math.sqrt(abs(math.pow(Doppler,2) - math.pow(doppler,2)))
    
    print("delta speed")
    print(f"X = {X}, Y = {Y}, Z = {Z}, Doppler = {Doppler}")

def calc_delta_speed_borring(data1,data2):
    X,Y,Z,Doppler = 0,0,0,0
    dx,dy,dz,dDoppler = 0,0,0,0

    
    x, y, z, doppler = data1
    X, Y, Z, Doppler = data2

    dx += math.sqrt(abs(math.pow(X,2) - math.pow(x,2)))
    dy += math.sqrt(abs(math.pow(Y,2) - math.pow(y,2)))
    dz += math.sqrt(abs(math.pow(Z,2) - math.pow(z,2)))
    dDoppler += math.sqrt(abs(math.pow(Doppler,2) - math.pow(doppler,2)))
    
    print("delta speed")
    print(f"X = {X}, Y = {Y}, Z = {Z}, Doppler = {Doppler}")
        





    
    

if __name__ == "__main__":
    f = open("OUT.bin", "rb").read()
    data = parse(f) # frames
    calculated = []

    for i,Data in enumerate(data):
        X,Y,Z,Doppler = calc_frame(Data)
        calculated.append((X,Y,Z,Doppler))
        print(f"frame = {i}, X = {X}, Y = {Y}, Z = {Z}, Doppler = {Doppler}")
    
    calc_delta_speed(calculated)


