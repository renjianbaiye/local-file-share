# Apple Style Frontend Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Restyle the Vue frontend so light mode is minimal Apple Photos inspired, dark mode is restrained immersive, and existing garbled Chinese UI copy is readable.

**Architecture:** Keep the current Vue/Vite app structure and route/component boundaries. Make the visual change through global design tokens in `frontend/src/style.css`, small component copy fixes, and minimal template changes in `App.vue` for theme-specific ambience.

**Tech Stack:** Vue 3, Vite, Vue Router, `@lucide/vue`, CSS variables, CSS media queries, existing `useTheme()` and `useEffects()` composables.

---

## File Map

- Modify `frontend/src/App.vue`: Replace garbled comments, keep mouse tracking, and keep the ambient layer gated by `canAnimate`.
- Modify `frontend/src/components/AppSidebar.vue`: Fix Chinese labels and titles, keep lucide icons and current route behavior.
- Modify `frontend/src/components/TopToolbar.vue`: Fix Chinese labels, placeholders, titles, and view labels.
- Modify `frontend/src/components/EmptyState.vue`: Fix default title text.
- Modify `frontend/src/views/SettingsView.vue`: Fix broken markup and Chinese labels in settings cards.
- Modify `frontend/src/style.css`: Replace the current over-decorated global styling with a cleaner Apple Photos light theme and a more immersive but controlled dark theme.
- Verify with `npm run build` from `frontend/`.

## Task 1: Fix Visible Chinese Copy

**Files:**
- Modify: `frontend/src/components/AppSidebar.vue`
- Modify: `frontend/src/components/TopToolbar.vue`
- Modify: `frontend/src/components/EmptyState.vue`
- Modify: `frontend/src/views/SettingsView.vue`

- [ ] **Step 1: Confirm current failure**

Run:

```powershell
Select-String -Path frontend\src\components\AppSidebar.vue,frontend\src\components\TopToolbar.vue,frontend\src\components\EmptyState.vue,frontend\src\views\SettingsView.vue -Pattern "�|鍥|璁|鏅|鐜|鎼|娣|娴"
```

Expected: matches in all four files, proving the visible copy is garbled.

- [ ] **Step 2: Replace sidebar copy**

In `frontend/src/components/AppSidebar.vue`, set:

```js
const mainNav = [
  { name: '图库', icon: Images, to: '/' },
  { name: '场景', icon: Clapperboard, to: '/scenes' },
  { name: '人物', icon: Users, to: '/people' },
  { name: '待整理', icon: Inbox, to: '/tidy' },
]

const toolNav = [
  { name: '文件传输', icon: FolderUp, to: '/files' },
  { name: '设置', icon: Settings, to: '/settings' },
]
```

Update template text and title:

```vue
<span class="brand-text">智能相册</span>

:title="theme === 'dark' ? '切换浅色模式' : '切换深色模式'"

<span>{{ theme === 'dark' ? '浅色模式' : '深色模式' }}</span>
```

- [ ] **Step 3: Replace toolbar copy**

In `frontend/src/components/TopToolbar.vue`, set:

```js
const views = [
  { id: 'timeline', icon: LayoutList, label: '时间流' },
  { id: 'masonry', icon: Columns3, label: '瀑布流' },
  { id: 'grid', icon: LayoutGrid, label: '网格' },
]
```

Update visible copy:

```vue
placeholder="搜索人物、场景、标签..."
title="重新扫描并加载最新照片"
<span>刷新索引</span>
<span>{{ isSelectMode ? '取消选择' : '选择' }}</span>
<span>智能整理</span>
```

- [ ] **Step 4: Replace empty-state default copy**

In `frontend/src/components/EmptyState.vue`, set:

```js
title: { type: String, default: '暂无内容' },
```

- [ ] **Step 5: Replace settings copy and fix broken template**

In `frontend/src/views/SettingsView.vue`, update visible text to:

```vue
<h1>设置</h1>
<p>个性化你的智能相册体验</p>
<h3><Monitor :size="16" /> 外观</h3>
<strong>主题模式</strong>
<p>当前使用 {{ theme === 'dark' ? '深色' : '浅色' }} 模式</p>
<span>切换为{{ theme === 'dark' ? '浅色' : '深色' }}</span>
<h3><Sparkles :size="16" /> 动效与动画</h3>
<strong>环境动效</strong>
<p>控制背景环境光、照片流动效和场景氛围粒子</p>
<p v-if="reducedMotion" class="setting-note">系统已开启“减少动态效果”，动效会自动停用。</p>
<strong>当前状态</strong>
{{ canAnimate ? '已启用' : '已停用' }}
<h3><Info :size="16" /> 关于</h3>
<strong>本地智能相册</strong>
<p>本地照片整理与浏览系统</p>
<p class="setting-note" style="color: var(--text-tertiary) !important; font-style: normal;">
  DINOv2-Large Tagger v3 · 42 标签体系 · Vue 3 + Vite
</p>
```

- [ ] **Step 6: Run build to catch markup errors**

Run:

```powershell
npm run build
```

from `frontend/`.

Expected: build passes. If it fails, fix only syntax/template errors introduced in this task.

## Task 2: Rebuild Theme Tokens and Ambient Background

**Files:**
- Modify: `frontend/src/App.vue`
- Modify: `frontend/src/style.css`

- [ ] **Step 1: Inspect current style anchors**

Run:

```powershell
Select-String -Path frontend\src\style.css -Pattern ":root|data-theme|ambient-bg|gradient-orb|app-shell|app-sidebar|top-toolbar|view-header|@media"
```

Expected: locate the existing token, ambient, layout, and responsive sections.

- [ ] **Step 2: Update `App.vue` comments only**

Change garbled comments to readable English comments:

```js
// Mouse tracking for subtle glass highlights.
```

and:

```vue
<!-- Ambient background is gated by the effects preference. -->
```

No behavior changes in this step.

- [ ] **Step 3: Replace global tokens**

In `frontend/src/style.css`, update `:root` and `:root[data-theme="dark"]` so light mode is minimal and dark mode is immersive:

```css
:root {
  --text-primary: #1d1d1f;
  --text-secondary: #6e6e73;
  --text-tertiary: #9a9aa0;
  --text-on-accent: #ffffff;
  --bg-page: #f5f5f7;
  --bg-sidebar: rgba(246, 246, 248, 0.78);
  --bg-glass: rgba(255, 255, 255, 0.62);
  --bg-glass-strong: rgba(255, 255, 255, 0.82);
  --bg-glass-solid: #ffffff;
  --bg-glass-hover: rgba(255, 255, 255, 0.94);
  --border-glass: rgba(0, 0, 0, 0.07);
  --border-sidebar: rgba(0, 0, 0, 0.08);
  --border-subtle: rgba(0, 0, 0, 0.05);
  --accent: #007aff;
  --accent-hover: #0071e3;
  --accent-light: rgba(0, 122, 255, 0.1);
  --accent-glow: rgba(0, 122, 255, 0.16);
}

:root[data-theme="dark"] {
  --text-primary: #f5f5f7;
  --text-secondary: #a1a1a6;
  --text-tertiary: #6e6e73;
  --bg-page: #050507;
  --bg-sidebar: rgba(22, 22, 25, 0.72);
  --bg-glass: rgba(35, 35, 39, 0.58);
  --bg-glass-strong: rgba(48, 48, 54, 0.72);
  --bg-glass-solid: #1c1c1e;
  --bg-glass-hover: rgba(58, 58, 64, 0.82);
  --border-glass: rgba(255, 255, 255, 0.1);
  --border-sidebar: rgba(255, 255, 255, 0.08);
  --border-subtle: rgba(255, 255, 255, 0.06);
  --accent: #0a84ff;
  --accent-hover: #409cff;
  --accent-light: rgba(10, 132, 255, 0.16);
  --accent-glow: rgba(10, 132, 255, 0.22);
}
```

Preserve existing state colors (`--red`, `--green`, `--orange`, `--purple`, `--teal`, `--pink`) unless their current values conflict with build or readability.

- [ ] **Step 4: Make ambient light theme quiet and dark theme immersive**

Update `.gradient-orb`, `.orb-1`, `.orb-2`, `.orb-3`, and dark overrides:

```css
.gradient-orb {
  position: absolute;
  border-radius: 50%;
  filter: blur(96px);
  opacity: 0.12;
  will-change: transform;
}

.orb-1 {
  width: 620px;
  height: 620px;
  top: -22%;
  left: -14%;
  background: radial-gradient(circle, rgba(0, 122, 255, 0.18) 0%, transparent 68%);
}

.orb-2 {
  width: 520px;
  height: 520px;
  top: 48%;
  right: -18%;
  background: radial-gradient(circle, rgba(255, 45, 85, 0.1) 0%, transparent 70%);
}

.orb-3 {
  width: 460px;
  height: 460px;
  bottom: -18%;
  left: 34%;
  background: radial-gradient(circle, rgba(52, 199, 89, 0.08) 0%, transparent 70%);
}

:root[data-theme="dark"] .gradient-orb {
  opacity: 0.34;
  filter: blur(110px);
}
```

Keep existing keyframes but reduce perceived motion by increasing durations if needed.

- [ ] **Step 5: Run build**

Run:

```powershell
npm run build
```

from `frontend/`.

Expected: build passes.

## Task 3: Refine Layout, Components, and Responsive Polish

**Files:**
- Modify: `frontend/src/style.css`
- Optionally modify: `frontend/src/views/SettingsView.vue` scoped styles if needed after build/manual inspection.

- [ ] **Step 1: Tighten app shell and sidebar styling**

In `frontend/src/style.css`, ensure:

```css
.app-sidebar {
  padding: 18px 12px;
  background: var(--bg-sidebar);
  backdrop-filter: blur(28px) saturate(160%);
  -webkit-backdrop-filter: blur(28px) saturate(160%);
}

.sidebar-item {
  min-height: 36px;
  border-radius: 10px;
}

.sidebar-item.active {
  background: var(--accent);
  color: var(--text-on-accent);
}

.view-page {
  padding: 28px 36px 96px;
  max-width: 1560px;
}
```

- [ ] **Step 2: Tighten toolbar controls**

Update toolbar styles to keep Apple-style compact controls:

```css
.top-toolbar {
  padding: 14px 0 12px;
  margin-bottom: 22px;
  background: color-mix(in srgb, var(--bg-page) 86%, transparent);
}

.toolbar-search {
  height: 36px;
  border-radius: var(--radius-full);
  background: var(--bg-glass);
}

.toolbar-btn {
  height: 34px;
  border-radius: var(--radius-full);
}
```

- [ ] **Step 3: Make cards and empty states quieter**

Adjust generic component styles:

```css
.empty-icon {
  background: var(--bg-glass-strong);
  box-shadow: var(--shadow-xs);
}

.settings-card,
.files-panel,
.memories-card,
.stat-item {
  background: var(--bg-glass);
  border: 1px solid var(--border-glass);
  box-shadow: var(--shadow-sm);
}
```

Avoid adding new nested cards.

- [ ] **Step 4: Preserve photo-first interaction**

Ensure photo-related hover styles remain:

```css
.masonry-item:hover img { transform: scale(1.035); }
.scene-card:hover,
.photo-stack-card:hover,
.person-card:hover { transform: translateY(-3px); }
```

The exact selectors may already exist; update values rather than duplicating selectors.

- [ ] **Step 5: Improve mobile behavior**

Keep the existing mobile breakpoint and ensure:

```css
@media (max-width: 768px) {
  .view-page { padding: 16px 16px 80px; }
  .top-toolbar { flex-wrap: wrap; }
  .toolbar-search { width: 100%; max-width: none; }
  .toolbar-actions { width: 100%; flex-wrap: wrap; }
  .settings-card .setting-row { align-items: flex-start; flex-direction: column; }
}
```

- [ ] **Step 6: Run final build**

Run:

```powershell
npm run build
```

from `frontend/`.

Expected: build passes with no Vue template errors.

## Task 4: Manual Verification and Cleanup

**Files:**
- No planned production file changes unless verification finds a concrete issue.

- [ ] **Step 1: Check git diff scope**

Run:

```powershell
git status --short
git diff -- frontend/src/App.vue frontend/src/components/AppSidebar.vue frontend/src/components/TopToolbar.vue frontend/src/components/EmptyState.vue frontend/src/views/SettingsView.vue frontend/src/style.css
```

Expected: only intended visual/copy files changed. Existing unrelated user changes remain untouched.

- [ ] **Step 2: Start local frontend if build passed**

Run:

```powershell
npm run dev -- --host 0.0.0.0
```

from `frontend/`.

Expected: Vite prints a local URL.

- [ ] **Step 3: Manual page checklist**

Open the Vite URL and check:

- 图库: light mode feels minimal and photos dominate.
- 场景: cards do not glow excessively.
- 人物: avatars and labels do not overlap.
- 待整理: stacked cards still read clearly.
- 文件传输: utility layout remains compact.
- 设置: Chinese text is readable, switches align on desktop and mobile.
- Dark mode: immersive dark background is visible but text remains readable.
- Effects off: continuous ambient animation disappears.

- [ ] **Step 4: Final status**

Report:

- Build result.
- Dev server URL if started.
- Any known limitations or manual checks that could not be completed.
