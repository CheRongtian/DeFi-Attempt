export interface Loadable<T> {
  data?: T
  loading: boolean
  error?: string
}
