import type { ReactNode } from 'react'

export function LoadingState({ label }: { label: string }) {
  return <div className="state-card state-loading" role="status" aria-live="polite">{label}</div>
}

export function ErrorState({ title, error }: { title: string; error: string }) {
  return (
    <div className="state-card state-error" role="alert">
      <strong>{title}</strong>
      <span>{error}</span>
    </div>
  )
}

export function EmptyState({ title, children }: { title: string; children: ReactNode }) {
  return (
    <div className="state-card">
      <strong>{title}</strong>
      <span>{children}</span>
    </div>
  )
}

export function StatusBadge({ tone, children }: { tone: 'good' | 'warning' | 'bad' | 'neutral'; children: ReactNode }) {
  return <span className={`status-badge status-${tone}`}>{children}</span>
}

export function Metric({ label, value, detail }: { label: string; value: ReactNode; detail?: ReactNode }) {
  return (
    <div className="metric">
      <dt>{label}</dt>
      <dd>{value}</dd>
      {detail ? <span>{detail}</span> : null}
    </div>
  )
}
