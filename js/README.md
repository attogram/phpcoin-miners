# Javascript Miner

This is a standalone Javascript miner that allows you to mine directly in your web browser. It connects to a specified node and uses your CPU to mine for new blocks.

## How to Use

To avoid CORS issues when running the miner locally, you need to use a local web server. This project includes a simple PHP proxy to handle API requests.

1.  **Start the PHP built-in web server:**

    Open your terminal, navigate to the `js` directory, and run the following command:

    ```bash
    php -S localhost:8000
    ```

2.  **Open the miner in your browser:**

    Navigate to `http://localhost:8000/miner.html` in your web browser.

3.  **Enter Miner Details:**
    *   **Node:** Enter the URL of the node you want to connect to.
    *   **Address:** Enter your miner address.
    *   **CPU:** Adjust the CPU usage slider to your desired level.

4.  **Start Mining:**

    Click the "Start" button to begin mining.

## Techy-Details

*   **Web Worker:** The miner uses a Web Worker to run the mining process in the background. This prevents the user interface from freezing while the miner is running.
*   **Hashing Algorithm:** The miner uses the Argon2i hashing algorithm, implemented using the `argon2-browser` library.
*   **Mining Process:**
    1.  The miner periodically fetches the latest block information from the specified node.
    2.  It then enters a loop, attempting to find a valid hash for the next block.
    3.  If a new block is found by the network, the miner will drop its current work and start again with the new block information.
    4.  When a valid block is found, it is submitted to the node.
