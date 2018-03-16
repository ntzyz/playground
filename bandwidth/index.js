const pcap = require('pcap')
const IPv4 = require('./node_modules/pcap/decode/ipv4');
const EthernetPacket = require('./node_modules/pcap/decode/ethernet_packet');

const session = pcap.createSession('br0');
const prefix = '172.16.10.';

const packet_size_map = {};

session.on('packet', raw_packet => {
  let packet;
  try {
    packet = pcap.decode.packet(raw_packet);
  } catch (_) {
    return;
  }
  if (packet.payload instanceof EthernetPacket &&
    packet.payload.payload instanceof IPv4) {
      let tcp_packet = packet.payload.payload;
      let addr;
      
      if (tcp_packet.saddr.addr.join('.').indexOf(prefix) === 0) {
        addr = tcp_packet.saddr.addr.join('.');
      } else {
        addr = tcp_packet.daddr.addr.join('.');
      }

      packet_size_map[addr] = (packet_size_map[addr] || 0) + tcp_packet.length;
    }
});

// From https://github.com/cnCalc/serainTalk/blob/dev/web/src/utils/filters.js
function fileSize (size) {
  const units = ['B', 'KB', 'MB', 'GB', 'TB', 'PB'];
  let n = Math.max(Math.floor(Math.log(size) / Math.log(1024)), 0);
  return `${(size / (1024 ** n)).toFixed(2)} ${units[n]}`;
}

setInterval(() => {
  process.stdout.write('\x1b[2J\x1b[H') // clear
  Object.keys(packet_size_map).sort().forEach(addr => {
    console.log(`${addr}\t${fileSize(packet_size_map[addr])}/s`);
    packet_size_map[addr] = 0;
  })
}, 1000);

