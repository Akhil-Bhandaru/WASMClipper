'use strict'

fs = require('fs')

result = null
do ->
  loadNativeClipper = (bytes) ->
    memory = new WebAssembly.Memory({
      initial: 256
      maximum: 256
    })
    env =
      abortStackOverflow: (err) -> throw new Error("overflow: #{err}")
      table: new WebAssembly.Table({
        initial: 0
        maximum: 0
        element: 'anyfunc'
      })
      __table_base: 0
      memory: memory
      __memory_base: 1024
      STACKTOP: 0
      STACK_MAX: memory.buffer.byteLength
    importObject = { env }
    return WebAssembly.instantiate(bytes, importObject)

  fs.readFile('../wasm/clipper.wasm', (err, data) ->
    if err then throw new Error(err)
    result = loadNativeClipper(new Uint8Array(data))
  )

module.exports = result.instance.exports
