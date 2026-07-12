// =============================================================================
// LianCore UI - 算法艺术可视化测试
// 覆盖: 波表/频谱/LFO模式切换、参数调节、Canvas渲染、导出
// =============================================================================
import { test, expect } from '@playwright/test';

test.describe('LianCore 算法艺术可视化', () => {

  test.beforeEach(async ({ page }) => {
    // 使用 file:// 协议直接打开静态HTML
    await page.goto('file:///f:/LianCore软音源合成器V3版本商业正式版/ui/visualizations/algorithmic_art.html');
    await page.waitForLoadState('networkidle');
    await page.waitForTimeout(1000);
  });

  // =========================================================================
  // 页面加载
  // =========================================================================
  test('页面加载 - 标题和画布可见', async ({ page }) => {
    await expect(page.getByText('LianCore')).toBeVisible();
    await expect(page.getByText('Algorithmic Art Visualizer')).toBeVisible();
    const canvas = page.locator('canvas');
    await expect(canvas).toBeVisible();
  });

  test('页面加载 - 侧边栏参数面板可见', async ({ page }) => {
    await expect(page.getByText('色彩主题')).toBeVisible();
    await expect(page.getByText('随机种子')).toBeVisible();
  });

  // =========================================================================
  // 模式切换
  // =========================================================================
  test('模式切换 - 默认显示波表模式', async ({ page }) => {
    const wavetableTab = page.locator('.mode-tab').filter({ hasText: '波表' });
    await expect(wavetableTab).toHaveClass(/active/);
    // 波表参数应可见
    await expect(page.getByText('谐波级数')).toBeVisible();
  });

  test('模式切换 - 切换到频谱模式', async ({ page }) => {
    await page.locator('.mode-tab').filter({ hasText: '频谱' }).click();
    await page.waitForTimeout(300);
    // 频谱参数应可见
    await expect(page.getByText('FFT 大小')).toBeVisible();
  });

  test('模式切换 - 切换到LFO模式', async ({ page }) => {
    await page.locator('.mode-tab').filter({ hasText: 'LFO' }).click();
    await page.waitForTimeout(300);
    // LFO参数应可见
    await expect(page.getByText('LFO 波形')).toBeVisible();
  });

  test('模式切换 - 标签激活状态切换', async ({ page }) => {
    const tabs = page.locator('.mode-tab');
    expect(await tabs.count()).toBe(3);

    // 点击频谱
    await tabs.filter({ hasText: '频谱' }).click();
    await expect(tabs.filter({ hasText: '频谱' })).toHaveClass(/active/);
    await expect(tabs.filter({ hasText: '波表' })).not.toHaveClass(/active/);

    // 点击LFO
    await tabs.filter({ hasText: 'LFO' }).click();
    await expect(tabs.filter({ hasText: 'LFO' })).toHaveClass(/active/);
    await expect(tabs.filter({ hasText: '频谱' })).not.toHaveClass(/active/);
  });

  // =========================================================================
  // 波表参数
  // =========================================================================
  test('波表 - 波形类型切换', async ({ page }) => {
    const slider = page.locator('#waveType');
    await expect(slider).toBeVisible();
    // 初始值
    const label = page.locator('#waveTypeLabel');
    await expect(label).toHaveText('谐波叠加');
    // 拖动到三角波
    await slider.fill('1');
    await expect(label).toHaveText('三角波');
  });

  test('波表 - 谐波级数调节', async ({ page }) => {
    const slider = page.locator('#harmonics');
    const label = page.locator('#harmonicsLabel');
    await slider.fill('16');
    await expect(label).toHaveText('16');
  });

  test('波表 - 波形变形参数', async ({ page }) => {
    const slider = page.locator('#warp');
    const label = page.locator('#warpLabel');
    await slider.fill('50');
    await expect(label).toHaveText('0.50');
  });

  test('波表 - 相位偏移参数', async ({ page }) => {
    const slider = page.locator('#phase');
    const label = page.locator('#phaseLabel');
    await slider.fill('25');
    await expect(label).toHaveText('0.25');
  });

  test('波表 - FM深度参数', async ({ page }) => {
    const slider = page.locator('#fm');
    const label = page.locator('#fmLabel');
    await slider.fill('75');
    await expect(label).toHaveText('0.75');
  });

  // =========================================================================
  // 频谱参数
  // =========================================================================
  test('频谱 - FFT大小调节', async ({ page }) => {
    await page.locator('.mode-tab').filter({ hasText: '频谱' }).click();
    await page.waitForTimeout(300);

    const slider = page.locator('#fftSize');
    const label = page.locator('#fftSizeLabel');
    await slider.fill('8');
    await expect(label).toHaveText('256');
  });

  test('频谱 - 滚动速度调节', async ({ page }) => {
    await page.locator('.mode-tab').filter({ hasText: '频谱' }).click();
    await page.waitForTimeout(300);

    const slider = page.locator('#scrollSpeed');
    const label = page.locator('#scrollSpeedLabel');
    await slider.fill('10');
    await expect(label).toHaveText('10');
  });

  // =========================================================================
  // LFO参数
  // =========================================================================
  test('LFO - 波形类型切换', async ({ page }) => {
    await page.locator('.mode-tab').filter({ hasText: 'LFO' }).click();
    await page.waitForTimeout(300);

    const slider = page.locator('#lfoWave');
    const label = page.locator('#lfoWaveLabel');
    await slider.fill('3');
    await expect(label).toHaveText('方波');
  });

  test('LFO - 数量调节', async ({ page }) => {
    await page.locator('.mode-tab').filter({ hasText: 'LFO' }).click();
    await page.waitForTimeout(300);

    const slider = page.locator('#lfoCount');
    const label = page.locator('#lfoCountLabel');
    await slider.fill('5');
    await expect(label).toHaveText('5');
  });

  test('LFO - 残影长度调节', async ({ page }) => {
    await page.locator('.mode-tab').filter({ hasText: 'LFO' }).click();
    await page.waitForTimeout(300);

    const slider = page.locator('#trail');
    const label = page.locator('#trailLabel');
    await slider.fill('100');
    await expect(label).toHaveText('100');
  });

  // =========================================================================
  // 通用控制
  // =========================================================================
  test('通用 - 暂停/恢复动画', async ({ page }) => {
    const pauseBtn = page.locator('#pauseBtn');
    await expect(pauseBtn).toHaveText('⏸ 暂停动画');
    await pauseBtn.click();
    await expect(pauseBtn).toHaveText('▶ 继续动画');
    await pauseBtn.click();
    await expect(pauseBtn).toHaveText('⏸ 暂停动画');
  });

  test('通用 - 色彩主题切换', async ({ page }) => {
    const slider = page.locator('#theme');
    const label = page.locator('#themeLabel');
    const themes = ['霓虹紫', '日出橙', '极光绿', '深海蓝', '赛博朋克'];
    for (const theme of themes) {
      const idx = themes.indexOf(theme);
      await slider.fill(String(idx));
      await expect(label).toHaveText(theme);
    }
  });

  test('通用 - 随机种子按钮', async ({ page }) => {
    const seedInput = page.locator('#seed');
    const initialSeed = await seedInput.inputValue();
    await page.locator('#randomizeBtn').click();
    const newSeed = await seedInput.inputValue();
    expect(newSeed).not.toBe(initialSeed);
  });

  test('通用 - 手动设置种子', async ({ page }) => {
    const seedInput = page.locator('#seed');
    await seedInput.fill('12345');
    await seedInput.dispatchEvent('change');
    await expect(seedInput).toHaveValue('12345');
  });

  test('通用 - 导出PNG按钮存在', async ({ page }) => {
    await expect(page.locator('#exportBtn')).toHaveText('📷 导出 PNG');
  });

  // =========================================================================
  // Canvas渲染
  // =========================================================================
  test('Canvas - 画布尺寸正确', async ({ page }) => {
    const canvas = page.locator('canvas');
    const box = await canvas.boundingBox();
    expect(box).not.toBeNull();
    if (box) {
      expect(box.width).toBe(800);
      expect(box.height).toBe(600);
    }
  });

  // =========================================================================
  // 快照
  // =========================================================================
  test('快照 - 波表模式截图', async ({ page }) => {
    await page.waitForTimeout(500);
    await page.screenshot({
      path: 'test-results/algorithmic-art-wavetable.png',
      fullPage: false,
    });
  });

  test('快照 - 频谱模式截图', async ({ page }) => {
    await page.locator('.mode-tab').filter({ hasText: '频谱' }).click();
    await page.waitForTimeout(1000);
    await page.screenshot({
      path: 'test-results/algorithmic-art-spectrogram.png',
      fullPage: false,
    });
  });

  test('快照 - LFO模式截图', async ({ page }) => {
    await page.locator('.mode-tab').filter({ hasText: 'LFO' }).click();
    await page.waitForTimeout(1000);
    await page.screenshot({
      path: 'test-results/algorithmic-art-lfo.png',
      fullPage: false,
    });
  });
});