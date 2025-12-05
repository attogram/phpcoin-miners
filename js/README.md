# Javascript Miner

This is a standalone Javascript miner that allows you to mine directly in your web browser. It connects to a specified node and uses your CPU to mine for new blocks.

## How to Use

1.  Open the `miner.html` file in your web browser.
2.  Enter the URL of the node you want to connect to.
3.  Enter your address.
4.  Adjust the CPU usage slider to your desired level.
5.  Click "Start" to begin mining.

## Techy-Details

*   **Web Worker:** The miner uses a Web Worker to run the mining process in the background. This prevents the user interface from freezing while the miner is running.
*   **Hashing Algorithm:** The miner uses the Argon2i hashing algorithm, implemented using the `argon2-browser` library.
*   **Mining Process:**
    1.  The miner periodically fetches the latest block information from the specified node.
    2.  It then enters a loop, attempting to find a valid hash for the next block.
    3.  If a new block is found by the network, the miner will drop its current work and start again with the new block information.
    4.  When a valid block is found, it is submitted to the node.
