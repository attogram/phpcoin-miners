const { test, expect } = require('@playwright/test');

test.describe('PHPCoin Web Miner', () => {
  test.beforeEach(async ({ page }) => {
    await page.goto('file://' + process.cwd() + '/js/miner.html');
  });

  test('should have the correct initial UI state', async ({ page }) => {
    await expect(page.locator('h1')).toHaveText('PHPCoin Web Miner v0.0.7');
    await expect(page.locator('#node')).toBeVisible();
    await expect(page.locator('#address')).toBeVisible();
    await expect(page.locator('#start')).toBeDisabled();
    await expect(page.locator('#stop')).toBeDisabled();
  });

  test('should enable start button only when both fields are filled', async ({ page }) => {
    await expect(page.locator('#start')).toBeDisabled();

    await page.fill('#node', 'http://localhost:8000');
    await expect(page.locator('#start')).toBeDisabled();

    await page.fill('#address', 'test-address');
    await expect(page.locator('#start')).toBeEnabled();

    await page.fill('#node', '');
    await expect(page.locator('#start')).toBeDisabled();
  });

  test('should start and stop the miner', async ({ page }) => {
    await page.fill('#node', 'http://localhost:8000');
    await page.fill('#address', 'test-address');

    await page.click('#start');
    await expect(page.locator('#start')).toBeDisabled();
    await expect(page.locator('#stop')).toBeEnabled();

    await page.click('#stop');
    await expect(page.locator('#start')).toBeEnabled();
    await expect(page.locator('#stop')).toBeDisabled();
  });

  test('should have working links after filling inputs', async ({ page }) => {
    await page.fill('#node', 'http://localhost:8000');
    await page.fill('#address', 'test-address');

    const nodeLink = await page.locator('#stat-node a').getAttribute('href');
    expect(nodeLink).toBe('http://localhost:8000');
    const minerLink = await page.locator('#stat-miner a').getAttribute('href');
    expect(minerLink).toBe('http://localhost:8000/apps/explorer/address.php?address=test-address');
  });

  test('should stop miner on node input change while running', async ({ page }) => {
    await page.fill('#node', 'http://localhost:8000');
    await page.fill('#address', 'test-address');

    await page.click('#start');
    await expect(page.locator('#start')).toBeDisabled();
    await expect(page.locator('#stop')).toBeEnabled();

    await page.fill('#node', 'http://localhost:8001');
    await expect(page.locator('#start')).toBeEnabled();
    await expect(page.locator('#stop')).toBeDisabled();
  });

  test('should stop miner on address input change while running', async ({ page }) => {
    await page.fill('#node', 'http://localhost:8000');
    await page.fill('#address', 'test-address');

    await page.click('#start');
    await expect(page.locator('#start')).toBeDisabled();
    await expect(page.locator('#stop')).toBeEnabled();

    await page.fill('#address', 'new-address');
    await expect(page.locator('#start')).toBeEnabled();
    await expect(page.locator('#stop')).toBeDisabled();
  });
});
