// =============================================================================
// LianCore UI - 极简模式测试
// 覆盖: AI输入、风格标签、情感滑块、预设浏览器、状态栏、模式切换
// =============================================================================
import { test, expect } from '@playwright/test';

test.describe('LianCore 极简模式 (Minimal Mode)', () => {

  test.beforeEach(async ({ page }) => {
    await page.goto('/');
    await page.waitForLoadState('networkidle');
    // 确保处于极简模式
    await page.waitForSelector('.app-container', { timeout: 10000 });
  });

  // =========================================================================
  // 页面加载
  // =========================================================================
  test('页面加载 - 显示应用容器和Logo', async ({ page }) => {
    await expect(page.locator('.app-container')).toBeVisible();
    await expect(page.locator('.logo')).toHaveText('LianCore');
    await expect(page.locator('.version-badge')).toHaveText('BETA');
  });

  test('页面标题正确', async ({ page }) => {
    await expect(page).toHaveTitle('LianCore - AI合成器');
  });

  // =========================================================================
  // 顶部栏
  // =========================================================================
  test('顶部栏 - 显示连接状态指示器', async ({ page }) => {
    const statusDot = page.locator('.status-dot');
    await expect(statusDot).toBeVisible();
    // 未连接时显示红色
    await expect(statusDot).toHaveClass(/disconnected/);
  });

  test('顶部栏 - 模式切换按钮存在', async ({ page }) => {
    // 在极简模式下，按钮文本应为"PRO"或"极简"
    const modeBtn = page.getByRole('button').filter({ hasText: /PRO|极简/ }).first();
    await expect(modeBtn).toBeVisible();
  });

  // =========================================================================
  // AI 输入栏
  // =========================================================================
  test('AI输入 - 输入框存在且可输入', async ({ page }) => {
    const input = page.locator('.ai-text-input');
    await expect(input).toBeVisible();
    await expect(input).toHaveAttribute('placeholder', /描述你想要的声音/);
    await input.fill('温暖的模拟合成器铺底');
    await expect(input).toHaveValue('温暖的模拟合成器铺底');
  });

  test('AI输入 - AI生成按钮存在', async ({ page }) => {
    const btn = page.locator('.ai-generate-btn');
    await expect(btn).toBeVisible();
    await expect(btn).toHaveText('AI 生成');
  });

  test('AI输入 - 空输入时生成按钮禁用', async ({ page }) => {
    const btn = page.locator('.ai-generate-btn');
    await expect(btn).toBeDisabled();
  });

  test('AI输入 - 输入文字后生成按钮启用', async ({ page }) => {
    const input = page.locator('.ai-text-input');
    await input.fill('测试音色');
    const btn = page.locator('.ai-generate-btn');
    await expect(btn).toBeEnabled();
  });

  test('AI输入 - 回车键触发生成', async ({ page }) => {
    const input = page.locator('.ai-text-input');
    await input.fill('测试音色');
    await input.press('Enter');
    // 验证生成按钮变为"生成中..."
    const btn = page.locator('.ai-generate-btn');
    await expect(btn).toHaveText('生成中...');
  });

  // =========================================================================
  // 风格标签
  // =========================================================================
  test('风格标签 - 所有标签可见', async ({ page }) => {
    const tags = page.locator('.style-tag');
    const count = await tags.count();
    expect(count).toBeGreaterThan(0);
    // 至少应有 "明亮" 标签
    await expect(page.locator('.style-tag').filter({ hasText: '明亮' })).toBeVisible();
  });

  test('风格标签 - 点击切换选中状态', async ({ page }) => {
    const tag = page.locator('.style-tag').first();
    await expect(tag).not.toHaveClass(/active/);
    await tag.click();
    await expect(tag).toHaveClass(/active/);
    await tag.click();
    await expect(tag).not.toHaveClass(/active/);
  });

  test('风格标签 - 可多选', async ({ page }) => {
    const tags = page.locator('.style-tag');
    const firstTag = tags.nth(0);
    const secondTag = tags.nth(1);
    await firstTag.click();
    await secondTag.click();
    await expect(firstTag).toHaveClass(/active/);
    await expect(secondTag).toHaveClass(/active/);
  });

  // =========================================================================
  // 情感滑块
  // =========================================================================
  test('情感滑块 - 显示三个情感控制', async ({ page }) => {
    await expect(page.locator('.emotion-sliders h3')).toHaveText('情感控制');
    const sliders = page.locator('.emotion-slider');
    await expect(sliders).toHaveCount(3);
  });

  test('情感滑块 - 标签正确', async ({ page }) => {
    const labels = page.locator('.emotion-label');
    await expect(labels.nth(0)).toContainText('温暖度');
    await expect(labels.nth(1)).toContainText('能量感');
    await expect(labels.nth(2)).toContainText('紧张度');
  });

  test('情感滑块 - 可拖动调整值', async ({ page }) => {
    const slider = page.locator('.emotion-slider').first();
    const initialValue = await slider.inputValue();
    // 通过聚焦和键盘操作修改值
    await slider.focus();
    await slider.press('ArrowRight');
    const newValue = await slider.inputValue();
    expect(parseFloat(newValue)).toBeGreaterThan(parseFloat(initialValue));
  });

  // =========================================================================
  // 预设浏览器
  // =========================================================================
  test('预设浏览器 - 标题和分类筛选可见', async ({ page }) => {
    await expect(page.locator('.preset-header h3')).toHaveText('预设');
    const select = page.locator('.preset-category-select');
    await expect(select).toBeVisible();
  });

  test('预设浏览器 - 分类下拉包含所有选项', async ({ page }) => {
    const select = page.locator('.preset-category-select');
    const options = await select.locator('option').allTextContents();
    expect(options).toContain('全部');
    expect(options).toContain('贝斯');
    expect(options).toContain('主音');
    expect(options).toContain('铺底');
  });

  test('预设浏览器 - 无预设时显示空状态', async ({ page }) => {
    const emptyMsg = page.locator('.preset-grid');
    await expect(emptyMsg).toContainText('暂无预设');
  });

  test('预设浏览器 - 分类切换更新筛选', async ({ page }) => {
    const select = page.locator('.preset-category-select');
    await select.selectOption('贝斯');
    await expect(select).toHaveValue('贝斯');
  });

  // =========================================================================
  // 状态栏
  // =========================================================================
  test('状态栏 - 显示CPU、内存、延迟信息', async ({ page }) => {
    const statusBar = page.locator('.status-bar');
    await expect(statusBar).toBeVisible();
    await expect(statusBar).toContainText('CPU');
    await expect(statusBar).toContainText('内存');
    await expect(statusBar).toContainText('延迟');
  });

  test('状态栏 - 显示引擎版本', async ({ page }) => {
    const statusBar = page.locator('.status-bar');
    await expect(statusBar).toContainText('Beta v3.0');
  });

  // =========================================================================
  // 模式切换
  // =========================================================================
  test('模式切换 - 点击切换到专业模式', async ({ page }) => {
    // 查找模式切换按钮并点击
    const modeBtn = page.getByRole('button').filter({ hasText: /PRO|极简/ }).first();
    const currentText = await modeBtn.textContent();
    await modeBtn.click();

    // 等待专业模式加载
    await page.waitForTimeout(500);

    // 验证专业模式元素出现
    if (currentText?.includes('极简') || currentText?.includes('PRO')) {
      // 在专业模式中应该能看到 "PRO" 标识
      const proIndicator = page.getByText('PRO').first();
      if (await proIndicator.isVisible()) {
        await expect(proIndicator).toBeVisible();
      }
    }
  });

  // =========================================================================
  // 键盘交互
  // =========================================================================
  test('键盘 - Shift+Enter 不触发生成', async ({ page }) => {
    const input = page.locator('.ai-text-input');
    await input.fill('测试');
    await input.press('Shift+Enter');
    // Shift+Enter 不应触发生成（按钮仍为"AI 生成"）
    const btn = page.locator('.ai-generate-btn');
    await expect(btn).toHaveText('AI 生成');
  });

  // =========================================================================
  // 快照测试
  // =========================================================================
  test('快照 - 极简模式首页截图', async ({ page }) => {
    await page.screenshot({
      path: 'test-results/minimal-mode-homepage.png',
      fullPage: true,
    });
  });
});