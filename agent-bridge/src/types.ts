// Shared contracts between the bridge, its provider adapters and the ThorVision client.

export type AgentId = 'codex' | 'claude' | 'gemini';

// Connection states are actionable for the user and never reported as callback failures.
export type ConnectionState =
  | 'connected'
  | 'signin_required'
  | 'quota_exhausted'
  | 'unsupported'
  | 'unavailable'
  | 'unknown'
  | 'error';

export interface Discovery {
  id: AgentId;
  name: string;
  available: boolean;
  version: string | null;
  runtime: string;
  problems: string[];
}

export interface AccountStatus {
  agent: AgentId;
  state: ConnectionState;
  /** Billing source exactly as the provider reports it, or "Unknown". */
  billing: string;
  account: string | null;
  detail: string;
}

export interface LoginInfo {
  /** Provider-owned browser sign-in page, when the provider offers one. */
  url?: string;
  /** One-time code to enter at `url`, for device-code flows. */
  code?: string;
  /** Interactive login command to run in a terminal, when no browser flow is available. */
  terminal?: { command: string; args: string[] };
  instructions: string;
}

export interface SessionOptions {
  camera: string;
  /** Extension project directory; generated files are confined to it. */
  workspace: string;
  /** Pre-deployed device API base with /analytics, or a local development API on loopback. */
  extensionsUrl: string;
}

export type AgentEvent =
  | { type: 'message'; text: string }
  | { type: 'progress'; text: string }
  | { type: 'toolStarted'; callId: string; tool: string; input?: unknown }
  | { type: 'toolFinished'; callId: string; tool: string; ok: boolean; summary?: string }
  | { type: 'permissionRequested'; requestId: string; tool: string; title: string; input?: unknown }
  | { type: 'artifactChanged'; path: string }
  | { type: 'completed'; stopReason: string }
  | { type: 'failed'; state: ConnectionState | 'agent_error'; message: string };

export interface PermissionRequest {
  tool: string;
  title: string;
  input?: unknown;
}

/** Resolves true to allow. The bridge routes it to the user; nothing is auto-approved here. */
export type PermissionAsker = (request: PermissionRequest) => Promise<boolean>;

export interface AgentSession {
  readonly id: string;
  sendPrompt(text: string): Promise<{ stopReason: string }>;
  cancel(): Promise<void>;
  close(): Promise<void>;
}

export interface AgentAdapter {
  readonly id: AgentId;
  readonly name: string;
  discover(): Promise<Discovery>;
  connect(): Promise<AccountStatus>;
  accountStatus(): Promise<AccountStatus>;
  login(): Promise<LoginInfo>;
  createSession(options: SessionOptions, emit: (event: AgentEvent) => void, ask: PermissionAsker): Promise<AgentSession>;
  /** Stops this bridge's provider processes; it never signs the user out of the provider. */
  disconnect(): Promise<void>;
}

/**
 * Child environment for subscription mode: provider API keys and endpoint overrides are
 * removed so the official runtime uses its own sign-in and never falls back to paid API
 * access. The user's global environment is not modified.
 */
export function subscriptionEnvironment(remove: string[], extra: Record<string, string> = {}): Record<string, string> {
  const environment: Record<string, string> = {};
  for (const [key, value] of Object.entries(process.env)) {
    if (value !== undefined && !remove.includes(key.toUpperCase())) environment[key] = value;
  }
  return { ...environment, ...extra };
}
