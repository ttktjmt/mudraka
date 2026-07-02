// Hand-written TypeScript definitions for the mudraka WASM package.
// The runtime surface is the embind binding in bindings/wasm/mudraka_embind.cpp.

/** Result of {@link Stream.pullInto}. */
export interface PullResult {
  /** samples written per channel into the destination region */
  written: number;
  /** cursor to pass to the next `pullInto` call */
  next_cursor: number;
  /** samples overwritten (lost) before this pull could read them */
  lost: number;
}

/** Opaque stream configuration. Free with {@link Config.delete} when done. */
export interface Config {
  delete(): void;
}

/** A decode stream — one per device connection. Free with {@link Stream.delete} when done. */
export interface Stream {
  /** Decode one BLE notification payload; returns samples written per channel (0 if malformed). */
  feed(data: Uint8Array, recvTimeS: number): number;
  /**
   * Drain samples since `cursor` into a WASM-heap region at `dstPtr`, laid out channel-major
   * as `channels * max` int32 (read via `HEAP32`). Up to `max` samples per channel.
   */
  pullInto(cursor: number, dstPtr: number, max: number): PullResult;
  /** Total samples pushed per channel since start (the newest absolute cursor). */
  head(): number;
  channels(): number;
  /** Estimated sample rate (Hz) from the clock model. */
  estimatedRateHz(): number;
  malformedFrames(): number;
  /** Device-clock microseconds of the last decoded notification (-1 if none yet). */
  lastDeviceTimeUs(): number;
  totalOverwritten(): number;
  /** Reconstructed host time (seconds) for absolute sample index `i`. */
  timestamp(i: number): number;
  /** Free the underlying C++ object. */
  delete(): void;
}

/** The instantiated mudraka module (an Emscripten Module with embind bindings). */
export interface MudrakaModule {
  /** Build a config from channels, nominal sample rate (Hz), and ring-buffer seconds. */
  makeConfig(channels: number, nominalRateHz: number, ringSeconds: number): Config;
  Config: { new (): Config };
  Stream: { new (config: Config): Stream };

  // Emscripten heap access (needed to allocate the pullInto destination).
  _malloc(size: number): number;
  _free(ptr: number): void;
  HEAP32: Int32Array;
  HEAPU8: Uint8Array;
}

/** Options for the module factory (e.g. `locateFile` to point at `mudraka.wasm`). */
export interface MudrakaModuleOptions {
  locateFile?: (path: string, scriptDirectory: string) => string;
  [key: string]: unknown;
}

/** Instantiate the WASM module. */
declare function createMudraka(options?: MudrakaModuleOptions): Promise<MudrakaModule>;
export default createMudraka;
