export function ConfigurationError({ issues }: { issues: string[] }) {
  return (
    <main className="configuration-error">
      <p className="eyebrow">Configuration required</p>
      <h1>Frontend configuration is incomplete</h1>
      <p>Copy <code>.env.example</code> to <code>.env.local</code> and resolve these values:</p>
      <ul>
        {issues.map((issue) => (
          <li key={issue}>{issue}</li>
        ))}
      </ul>
    </main>
  )
}
