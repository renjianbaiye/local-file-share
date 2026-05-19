<script setup>
import { computed, onMounted, ref } from 'vue'
import {
  ArrowDownToLine,
  ChevronRight,
  Copy,
  FileArchive,
  FileImage,
  FileText,
  Folder,
  Laptop,
  QrCode,
  RefreshCw,
  Search,
  ShieldCheck,
  Smartphone,
  Wifi,
  Upload,
} from '@lucide/vue'

const currentPath = ref('')
const parentPath = ref(null)
const accessUrl = ref('')
const entries = ref([])
const stats = ref({ folders: 0, files: 0, totalSize: '0 B' })
const loading = ref(false)
const isUploading = ref(false)
const errorMessage = ref('')
const query = ref('')

const visibleEntries = computed(() => {
  const keyword = query.value.trim().toLowerCase()
  if (!keyword) return entries.value
  return entries.value.filter((entry) => entry.name.toLowerCase().includes(keyword))
})

const statCards = computed(() => [
  { value: stats.value.folders ?? 0, label: '文件夹' },
  { value: stats.value.files ?? 0, label: '文件' },
  { value: stats.value.totalSize ?? '0 B', label: '可下载总量' },
])

const devices = computed(() => [
  { name: 'C++ 服务', meta: accessUrl.value ? '正在运行' : '等待连接', icon: Laptop },
  { name: '移动设备', meta: '同一 Wi-Fi 扫码访问', icon: Smartphone },
])

const apiPath = (path = '') => `/api/list${path ? `?path=${encodeURIComponent(path)}` : ''}`

const loadDirectory = async (path = '') => {
  loading.value = true
  errorMessage.value = ''

  try {
    const response = await fetch(apiPath(path))
    if (!response.ok) throw new Error(`HTTP ${response.status}`)
    const data = await response.json()
    currentPath.value = data.currentPath || ''
    parentPath.value = data.parentPath ?? null
    accessUrl.value = data.accessUrl || ''
    entries.value = data.entries || []
    stats.value = data.stats || { folders: 0, files: 0, totalSize: '0 B' }
  } catch (error) {
    errorMessage.value = '没有连接到 C++ 后端。请先启动 LocalFileShare.exe，并确认 Vite 代理端口是 8080。'
  } finally {
    loading.value = false
  }
}

const handleUpload = async (event) => {
  const files = event.target.files
  if (!files || files.length === 0) return

  isUploading.value = true
  errorMessage.value = ''

  const formData = new FormData()
  for (let i = 0; i < files.length; i++) {
    formData.append('file', files[i])
  }

  try {
    const response = await fetch(`/api/upload?path=${encodeURIComponent(currentPath.value)}`, {
      method: 'POST',
      body: formData
    })

    if (!response.ok) {
      const msg = await response.text()
      throw new Error(msg || `HTTP ${response.status}`)
    }
    
    await loadDirectory(currentPath.value)
  } catch (error) {
    errorMessage.value = `上传失败: ${error.message}`
  } finally {
    isUploading.value = false
    event.target.value = ''
  }
}

const openEntry = (entry) => {
  if (entry.type === 'folder') {
    loadDirectory(entry.path)
    return
  }
  window.location.href = entry.url
}

const openQr = () => {
  window.open('/qr', '_blank', 'noopener,noreferrer')
}

const copyUrl = async () => {
  if (!accessUrl.value) return
  await navigator.clipboard?.writeText(accessUrl.value)
}

const fileType = (entry) => {
  if (entry.type === 'folder') return 'folder'
  const lower = entry.name.toLowerCase()
  if (/\.(zip|rar|7z|tar|gz)$/.test(lower)) return 'archive'
  if (/\.(png|jpg|jpeg|gif|webp|svg|heic)$/.test(lower)) return 'image'
  return 'document'
}

const fileIcon = (entry) => {
  const type = fileType(entry)
  if (type === 'folder') return Folder
  if (type === 'archive') return FileArchive
  if (type === 'image') return FileImage
  return FileText
}

onMounted(() => loadDirectory())
</script>

<template>
  <main class="shell">
    <nav class="nav">
      <a class="brand" href="#" aria-label="LocalFileShare 首页" @click.prevent="loadDirectory()">
        <span class="brand-mark">
          <Wifi :size="19" />
        </span>
        <span>LocalFileShare</span>
      </a>
      <div class="nav-actions">
        <label class="search-box" aria-label="搜索文件">
          <Search :size="17" />
          <input v-model="query" type="search" placeholder="搜索文件" />
        </label>
        <button class="primary-button" type="button" @click="loadDirectory(currentPath)">
          <RefreshCw :size="18" />
          刷新
        </button>
      </div>
    </nav>

    <section class="hero">
      <div class="hero-copy">
        <p class="eyebrow">LAN AIR SHARE</p>
        <h1>像 AirDrop 一样，把文件送到身边的设备。</h1>
        <p class="lead">
          Vue 页面现在会连接 C++ 后端读取真实共享目录。扫码访问、目录跳转和文件下载都走同一个
          LocalFileShare 服务。
        </p>

        <div class="hero-actions">
          <button class="hero-button dark" type="button" @click="openQr">
            <QrCode :size="18" />
            显示二维码
          </button>
          <button class="hero-button light" type="button" :disabled="!accessUrl" @click="copyUrl">
            <Copy :size="18" />
            复制访问链接
          </button>
        </div>

        <div class="stats-grid">
          <div v-for="item in statCards" :key="item.label" class="stat-card">
            <strong>{{ item.value }}</strong>
            <span>{{ item.label }}</span>
          </div>
        </div>
      </div>

      <aside class="phone-panel" aria-label="扫码访问卡片">
        <div class="phone-frame">
          <div class="phone-top">
            <span></span>
            <span></span>
          </div>
          <div class="qr-card">
            <img v-if="accessUrl" :src="'/qr.svg?v=2'" alt="QR code" />
            <QrCode v-else :size="112" stroke-width="1.3" />
          </div>
          <h2>扫码访问</h2>
          <p>手机、平板和电脑在同一 Wi-Fi 下即可打开。</p>
          <div class="url-pill">{{ accessUrl || '等待 C++ 服务启动' }}</div>
        </div>
      </aside>
    </section>

    <section class="workspace">
      <aside class="side-panel">
        <div class="panel-title">
          <ShieldCheck :size="19" />
          连接状态
        </div>
        <p class="panel-copy">
          前端运行在 Vite，文件访问由 C++ 服务提供。开发时通过 Vite 代理把 /api、/download、/qr
          转发到后端。
        </p>

        <div class="device-list">
          <article v-for="device in devices" :key="device.name" class="device-card">
            <component :is="device.icon" :size="20" />
            <div>
              <strong>{{ device.name }}</strong>
              <span>{{ device.meta }}</span>
            </div>
          </article>
        </div>
      </aside>

      <section class="file-panel">
        <header class="file-head">
          <div>
            <p class="section-kicker">共享目录</p>
            <h2>/{{ currentPath }}</h2>
          </div>
          <div style="display: flex; gap: 10px; align-items: center;">
            <label class="ghost-button" style="cursor: pointer; margin: 0;">
              <Upload :size="17" />
              {{ isUploading ? '上传中...' : '上传文件' }}
              <input type="file" multiple @change="handleUpload" style="display: none;" :disabled="isUploading" />
            </label>
            <button v-if="parentPath !== null" class="ghost-button" type="button" @click="loadDirectory(parentPath)" style="margin: 0;">
              返回上级
              <ChevronRight :size="17" />
            </button>
          </div>
        </header>

        <div v-if="errorMessage" class="notice error">
          {{ errorMessage }}
        </div>
        <div v-else-if="loading" class="notice">
          正在读取 C++ 后端目录...
        </div>
        <div v-else-if="visibleEntries.length === 0" class="notice">
          当前目录没有可显示的文件。
        </div>

        <div v-else class="file-list">
          <article
            v-for="entry in visibleEntries"
            :key="entry.path"
            class="file-row"
            role="button"
            tabindex="0"
            @click="openEntry(entry)"
            @keydown.enter="openEntry(entry)"
          >
            <span class="file-icon" :class="fileType(entry)">
              <component :is="fileIcon(entry)" :size="21" />
            </span>
            <div class="file-main">
              <strong>{{ entry.name }}</strong>
              <span>{{ entry.type === 'folder' ? '文件夹' : '点击下载' }}</span>
            </div>
            <span class="file-size">{{ entry.size }}</span>
            <button class="download-button" type="button" :aria-label="entry.type === 'folder' ? '进入目录' : '下载文件'">
              <ChevronRight v-if="entry.type === 'folder'" :size="18" />
              <ArrowDownToLine v-else :size="18" />
            </button>
          </article>
        </div>
      </section>
    </section>
  </main>
</template>
