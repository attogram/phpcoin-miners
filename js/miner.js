importScripts('argon2-bundled.min.js');
importScripts('web-miner.js');

let webMiner;

self.addEventListener('message', function(e) {
    const { cmd, params } = e.data;

    if (cmd === 'INIT') {
        const { node, address, options } = params;
        webMiner = new WebMiner(node, address, options, {
            onMinerUpdate(data) {
                self.postMessage({ cmd: 'EVENT', event: 'onMinerUpdate', data });
            },
            onAccepted(response) {
                self.postMessage({ cmd: 'EVENT', event: 'onAccepted', data: JSON.stringify(response) });
            },
            onRejected(response) {
                self.postMessage({ cmd: 'EVENT', event: 'onRejected', data: JSON.stringify(response) });
            },
            saveStat(data) {
                self.postMessage({ cmd: 'EVENT', event: 'saveStat', data });
            }
        });
    } else if (cmd === 'START') {
        if (webMiner) {
            webMiner.start();
            self.postMessage({ cmd: 'STARTED' });
        }
    } else if (cmd === 'STOP') {
        if (webMiner) {
            webMiner.stop();
        }
    } else if (cmd === 'checkAddress') {
        if (webMiner) {
            const { address } = params;
            webMiner.checkAddress(address).then(response => {
                self.postMessage({ cmd: 'checkAddressResponse', response });
            });
        }
    } else if (cmd === 'updateCpu') {
        if (webMiner) {
            const { cpu } = params;
            webMiner.updateCpu(cpu);
        }
    } else if (cmd === 'resetStat') {
        if (webMiner) {
            webMiner.resetStat();
        }
    }
}, false);
