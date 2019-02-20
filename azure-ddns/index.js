const DnsManagementClient = require('azure-arm-dns');
const msRestAzure = require("ms-rest-azure");
const express = require('express');
const site = express();

const subscriptionId = '<---->';
const resourcesGroup = '<---->';
const zoneName = '<---->';
let dnsClient = null;

site.post('/update_dns', (req, res) => {
  if (!req.query.ip_addr) {
    res.send({ status: 'ok' });
  } else {
    dnsClient.recordSets.update(resourcesGroup, zoneName, 'prefix.', 'A', {
      aRecords: [{
        ipv4Address: req.query.ip_addr
      }]
    }).then((response) => {
      res.send({
        status: 'ok',
        azure_response: response,
      });
    }).catch((error) => {
      res.send({
        status: 'error',
        azure_error: error,
      });
    })
  }
});

msRestAzure.interactiveLogin({
  domain: '<---->'
}).then((creds) => {
  dnsClient = new DnsManagementClient(creds, subscriptionId);
  site.listen(3399, '127.0.0.1');
}).catch((error) => {
  console.error(error);
})
