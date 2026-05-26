import { ref, watch, onMounted } from 'vue'

const theme = ref('light')

export function useTheme() {
  const init = () => {
    const saved = localStorage.getItem('album-theme')
    if (saved) {
      theme.value = saved
    } else if (window.matchMedia?.('(prefers-color-scheme: dark)').matches) {
      theme.value = 'dark'
    }
    apply()
  }

  const apply = () => {
    document.documentElement.setAttribute('data-theme', theme.value)
  }

  const toggle = () => {
    theme.value = theme.value === 'light' ? 'dark' : 'light'
    localStorage.setItem('album-theme', theme.value)
    apply()
  }

  const isDark = () => theme.value === 'dark'

  onMounted(init)
  watch(theme, apply)

  return { theme, toggle, isDark }
}
