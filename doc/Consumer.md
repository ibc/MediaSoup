# Consumer

## RTP sequence number handling

---

INIT:

- rtpLastSeq: 1000
- rtpPreviousBaseSeq: 0
- rtpBaseSeq: 0

---

packet (SYNC):

- seq: 3000
- rtpPreviousBaseSeq: 1000
- rtpBaseSeq: 3000

out:

- rtpLastSeq: 3000 - 3000 + 1000 + 1 = 1001

---

packet:

- seq: 3001

out:

- rtpLastSeq: 3001 - 3000 + 1000 + 1 = 1002

---

packet:

- seq: 3003

out:

- rtpLastSeq: 3003 - 3000 + 1000 + 1 = 1004

---

packet (SYNC):

- seq: 4050

* rtpPreviousBaseSeq: 1004
* rtpBaseSeq: 4050

out:

- rtpLastSeq: 4050 - 4050 + 1004 + 1 = 1005

---

packet:

- seq: 4051

out:

- rtpLastSeq: 4051 - 4050 + 1004 + 1 = 1006

---

packet (DROP):

- seq: 4052

if (seq > rtpLastSeq)

- rtpBaseSeq++: 4051

---

packet:

- seq: 4053

out:

- rtpLastSeq: 4053 - 4051 + 1004 + 1 = 1007

---

probation packet:

- rtpLastSeq++ = 1008

out:

- rtpLastSeq: 1008
- rtpBaseSeq--: 4050

NOTE: probation packets should just be sent (assuming same RTP timestamp) **after** a video packet with `marker` bit set to 1.

---

packet:

- seq: 4054

out:

- rtpLastSeq: 4054 - 4050 + 1004 + 1 = 1009
