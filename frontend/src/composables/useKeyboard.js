import { onMounted, onBeforeUnmount } from 'vue'

export function useKeyboard(handlers = {}) {
  const handle = (e) => {
    // Don't trigger shortcuts when typing in inputs
    const tag = e.target.tagName
    if (tag === 'INPUT' || tag === 'TEXTAREA' || tag === 'SELECT') {
      if (e.key === 'Escape') {
        e.target.blur()
      }
      return
    }

    const key = e.key
    const ctrl = e.ctrlKey || e.metaKey

    if (key === 'Escape' && handlers.onEscape) {
      e.preventDefault()
      handlers.onEscape()
    } else if (key === ' ' && handlers.onSpace) {
      e.preventDefault()
      handlers.onSpace()
    } else if (key === '/' && handlers.onSearch) {
      e.preventDefault()
      handlers.onSearch()
    } else if (key === 'd' && !ctrl && handlers.onDelete) {
      handlers.onDelete()
    } else if (key === 'k' && !ctrl && handlers.onKeep) {
      handlers.onKeep()
    } else if (key === 'a' && ctrl && handlers.onSelectAll) {
      e.preventDefault()
      handlers.onSelectAll()
    } else if (key === 'ArrowLeft' && handlers.onPrev) {
      e.preventDefault()
      handlers.onPrev()
    } else if (key === 'ArrowRight' && handlers.onNext) {
      e.preventDefault()
      handlers.onNext()
    }
  }

  onMounted(() => window.addEventListener('keydown', handle))
  onBeforeUnmount(() => window.removeEventListener('keydown', handle))
}
