## Bandwidth

Find out who is consuming most of our bandwidth. Sample output:

```
172.16.10.1     0.00 B/s
172.16.10.112   0.00 B/s
172.16.10.170   0.00 B/s
172.16.10.215   7.39 MB/s
172.16.10.247   0.00 B/s
172.16.10.41    0.00 B/s
172.16.10.81    0.00 B/s
224.0.0.1       0.00 B/s
```

### Useage

If you believe your router is powerful enough to run node.js, just run it on your router:

1. Install `python`, `git`, `git-http`, `node`, `gcc` with `opkg`.
2. Download the source code of `libpacp`, compile and install it.
3. Install node packages by `npm i`.
3. Run `index.js` with root privilege.

Or, you can:

1. Install `iptables-mod-tee` on your OpenWrt/LEDE router.
2. Run `iptables -I FORWARD -j TEE --gateway $(echo $SSH_CLIENT | awk '{ print $1 }')` on your OpenWrt/LEDE router.
3. Run `index.js` with root privilege.
4. Run `iptables -D FORWARD -j TEE --gateway $(echo $SSH_CLIENT | awk '{ print $1 }')` on your OpenWrt/LEDE router.
