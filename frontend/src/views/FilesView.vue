<script setup>
import { ref, computed, onMounted, onBeforeUnmount } from 'vue'
import {
  ChevronRight, Copy, Upload, RefreshCw, Search,
  Folder, FileArchive, FileImage, FileText, ArrowDownToLine, QrCode,
} from '@lucide/vue'

const currentPath = ref('')
const parentPath = ref(null)
const accessUrl = ref('')
const entries = ref([])
const fileStats = ref({ folders: 0, files: 0, totalSize: '0 B' })
const fileLoading = ref(false)
const isUploading = ref(false)
const fileError = ref('')
const fileQuery = ref('')
const debouncedFileQuery = ref('')
let fileSearchTimer = null
let uploadRefreshTimer = null
let uploadRefreshInFlight = false

const visibleEntries = computed(() => {
  const keyword = debouncedFileQuery.value.trim().toLowerCase()
  if (!keyword) return entries.value
  return entries.value.filter((entry) => entry.name.toLowerCase().includes(keyword))
})

import { watch } from 'vue'
watch(fileQuery, (value) => {
  if (fileSearchTimer) clearTimeout(fileSearchTimer)
  fileSearchTimer = setTimeout(() => { debouncedFileQuery.value = value }, 150)
})

const apiPath = (path = '') => `/api/list${path ? `?path=${encodeURIComponent(path)}` : ''}`

const loadDirectory = async (path = '') => {
  fileLoading.value = true
  fileError.value = ''
  try {
    const response = await fetch(apiPath(path))
    if (!response.ok) throw new Error(`HTTP ${response.status}`)
    const data = await response.json()
    currentPath.value = data.currentPath || ''
    parentPath.value = data.parentPath ?? null
    accessUrl.value = data.accessUrl || ''
    entries.value = data.entries || []
    fileStats.value = data.stats || { folders: 0, files: 0, totalSize: '0 B' }
  } catch (error) {
    fileError.value = `无法读取目录: ${error.message}`
  } finally {
    fileLoading.value = false
  }
}

const refreshDirectorySnapshot = async (path = currentPath.value) => {
  if (uploadRefreshInFlight) return
  uploadRefreshInFlight = true
  try {
    const response = await fetch(apiPath(path))
    if (!response.ok) return
    const data = await response.json()
    if ((data.currentPath || '') !== currentPath.value) return
    parentPath.value = data.parentPath ?? null
    accessUrl.value = data.accessUrl || ''
    entries.value = data.entries || []
    fileStats.value = data.stats || { folders: 0, files: 0, totalSize: '0 B' }
  } catch { /* keep upload error focused */ }
  finally { uploadRefreshInFlight = false }
}

const startUploadRefresh = () => {
  if (uploadRefreshTimer) clearInterval(uploadRefreshTimer)
  uploadRefreshTimer = setInterval(() => void refreshDirectorySnapshot(), 3000)
}

const stopUploadRefresh = () => {
  if (!uploadRefreshTimer) return
  clearInterval(uploadRefreshTimer)
  uploadRefreshTimer = null
}

const uploadFile = async (file) => {
  const formData = new FormData()
  formData.append('file', file)
  const response = await fetch(`/api/upload?path=${encodeURIComponent(currentPath.value)}`, {
    method: 'POST', body: formData,
  })
  if (!response.ok) {
    const msg = await response.text()
    throw new Error(`${file.name}: ${msg || `HTTP ${response.status}`}`)
  }
}

const handleUpload = async (event) => {
  const files = Array.from(event.target.files || [])
  if (files.length === 0) return
  isUploading.value = true
  fileError.value = ''
  startUploadRefresh()
  try {
    for (const file of files) {
      await uploadFile(file)
      await refreshDirectorySnapshot()
    }
    await loadDirectory(currentPath.value)
  } catch (error) {
    fileError.value = `上传失败: ${error.message}`
  } finally {
    stopUploadRefresh()
    isUploading.value = false
    event.target.value = ''
  }
}

const openEntry = (entry) => {
  if (entry.type === 'folder') { loadDirectory(entry.path); return }
  window.location.href = entry.url
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

onMounted(async () => {
  try {
    const url = new URL(window.location.href)
    const urlToken = url.searchParams.get('token')
    const token = urlToken || import.meta.env.VITE_LFS_TOKEN
    if (token) {
      await fetch(`/api/auth?token=${encodeURIComponent(token)}`)
      if (urlToken) {
        url.searchParams.delete('token')
        window.history.replaceState({}, document.title, `${url.pathname}${url.search}${url.hash}`)
      }
    }
    await loadDirectory()
  } catch (error) {
    fileError.value = `初始化失败: ${error.message}`
  }
})

onBeforeUnmount(() => {
  if (fileSearchTimer) clearTimeout(fileSearchTimer)
  stopUploadRefresh()
})
</script>

<template>
  <div class="view-page files-view">
    <header class="view-header">
      <h1>文件传输</h1>
      <p>在局域网内共享和传输文件</p>
    </header>

    <div class="files-layout">
      <aside class="files-sidebar">
        <div class="files-path">当前路径: /{{ currentPath }}</div>
        <div class="files-stats">
          <span>{{ fileStats.folders ?? 0 }} 个文件夹</span>
          <span>{{ fileStats.files ?? 0 }} 个文件</span>
          <span>共 {{ fileStats.totalSize ?? '0 B' }}</span>
        </div>

        <label class="files-upload-btn">
          <Upload :size="16" />
          {{ isUploading ? '上传中...' : '上传文件' }}
          <input type="file" multiple :disabled="isUploading" @change="handleUpload" />
        </label>

        <div class="files-actions">
          <button class="action-btn" :disabled="!accessUrl" @click="copyUrl">
            <Copy :size="14" /> 复制链接
          </button>
          <a class="action-btn" href="/qr" target="_blank" rel="noreferrer">
            <QrCode :size="14" /> 二维码
          </a>
        </div>
      </aside>

      <section class="files-panel">
        <header class="files-panel-head">
          <label class="toolbar-search">
            <Search :size="16" />
            <input v-model="fileQuery" type="search" placeholder="搜索当前文件夹..." />
          </label>
          <div class="files-head-actions">
            <button v-if="parentPath !== null" class="action-btn" @click="loadDirectory(parentPath)">返回上级</button>
            <button class="action-btn primary" @click="loadDirectory(currentPath)">
              <RefreshCw :size="14" /> 刷新
            </button>
          </div>
        </header>

        <div v-if="fileError" class="notice error">{{ fileError }}</div>
        <div v-else-if="fileLoading" class="notice">正在加载文件列表...</div>
        <div v-else-if="visibleEntries.length === 0" class="notice">当前文件夹为空</div>

        <div v-else class="file-list">
          <article
            v-for="(entry, idx) in visibleEntries"
            :key="entry.path"
            class="file-row"
            :style="{ '--i': idx }"
            @click="openEntry(entry)"
          >
            <span class="file-icon" :class="fileType(entry)">
              <component :is="fileIcon(entry)" :size="20" />
            </span>
            <div class="file-main">
              <strong>{{ entry.name }}</strong>
              <span>{{ entry.type === 'folder' ? '文件夹' : '文件' }}</span>
            </div>
            <span class="file-size">{{ entry.size }}</span>
            <span class="row-action">
              <ChevronRight v-if="entry.type === 'folder'" :size="16" />
              <ArrowDownToLine v-else :size="16" />
            </span>
          </article>
        </div>
      </section>
    </div>
  </div>
</template>
