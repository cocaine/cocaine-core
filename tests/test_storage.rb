require 'cocaine'
require 'test/unit'

Celluloid.logger = nil
Cocaine::LOG.level = Logger::ERROR

COLLECTION_ACLS = '.collection-acls'
UIDS = {
  :random => '42',
  :esafronov => '1120000000015084',
}

s = Cocaine::Service.new 'storage'

RSpec.describe Cocaine::Service do
  it 'cleans up' do
    paths = [
      ['secrets', 'key'],
      ['collection', 'key'],
      [COLLECTION_ACLS, 'secrets'],
      [COLLECTION_ACLS, 'collection'],
      [COLLECTION_ACLS, COLLECTION_ACLS],
    ]

    paths.each do |collection, key|
      tx, rx = s.remove(collection, key,
        :authorization => UIDS[:esafronov]
      )
      id, data = rx.recv()
      expect(id).to eq(:value)
      expect(data).to eq([])
    end
  end

  context 'common collection' do
    it 'creates common (uncaptured) collection' do
      tx, rx = s.write('collection', 'key', 'Путин - краб!', [])
      id, data = rx.recv()

      expect(id).to eq(:value)
      expect(data).to eq([])
    end

    it 'reads from common (uncaptured) collection' do
      tx, rx = s.read('collection', 'key')
      id, data = rx.recv()

      expect(id).to eq(:value)
      expect(data).to eq(['Путин - краб!'])
    end

    it 'reads from common (uncaptured) collection with credentials' do
      tx, rx = s.read('collection', 'key',
        :authorization => UIDS[:esafronov]
      )
      id, data = rx.recv()

      expect(id).to eq(:value)
      expect(data).to eq(['Путин - краб!'])
    end

    # Test that previous read operation did not capture the collection.
    it 'reads from common (uncaptured) collection with anonymous again' do
      tx, rx = s.read('collection', 'key')
      id, data = rx.recv()

      expect(id).to eq(:value)
      expect(data).to eq(['Путин - краб!'])
    end

    it 'reads from common (uncaptured) non-existing collection' do
      tx, rx = s.read('not-existing-collection', 'key')
      id, data = rx.recv()

      expect(id).to eq(:error)
    end

    # And after this test the collection is captured.
    it 'writes again into common (uncaptured) collection with credentials' do
      tx, rx = s.write('collection', 'key', 'Путин - не краб!', [],
        :authorization => UIDS[:esafronov]
      )
      id, data = rx.recv()

      expect(id).to eq(:value)
      expect(data).to eq([])
    end

    it 'reads from common just captured collection with credentials' do
      tx, rx = s.read('collection', 'key',
        :authorization => UIDS[:esafronov]
      )
      id, data = rx.recv()

      expect(id).to eq(:value)
      expect(data).to eq(['Путин - не краб!'])
    end

    it 'fails to read from common just captured collection without credentials' do
      tx, rx = s.read('collection', 'key')
      id, data = rx.recv()

      expect(id).to eq(:error)
      expect(data).to eq([[12, 13], 'Permission denied'])
    end
  end

  context 'owned collection for user `esafronov`' do
    it 'creates secret collection' do
      tx, rx = s.write('secrets', 'key', 'ks/Os51X2RTtixTQ43ZD3geXrlY=', [],
        :authorization => UIDS[:esafronov]
      )
      id, data = rx.recv()

      expect(id).to eq(:value)
      expect(data).to eq([])
    end

    it 'forbids to read secret collection for user `random`' do
      tx, rx = s.read('secrets', 'key',
        :authorization => UIDS[:random]
      )
      id, data = rx.recv()

      expect(id).to eq(:error)
      expect(data).to eq([[12, 13], 'Permission denied'])
    end

    it 'forbids to add permissions for user `random` with user `random` credentials' do
      perm = {
        UIDS[:random].to_i => 0x01 | 0x02,
      }

      tx, rx = s.write(COLLECTION_ACLS, 'secrets', MessagePack.pack(perm), [],
        :authorization => UIDS[:random]
      )
      id, data = rx.recv()

      expect(id).to eq(:error)
      expect(data).to eq([[12, 13], 'Permission denied'])
    end

    it 'adds read permissions for user `random` with user `esafronov` credentials' do
      perm = {
        UIDS[:random].to_i => 0x01,
        UIDS[:esafronov].to_i => 0x01 | 0x02,
      }

      tx, rx = s.write(COLLECTION_ACLS, 'secrets', MessagePack.pack(perm), [],
        :authorization => UIDS[:esafronov]
      )
      id, data = rx.recv()

      expect(id).to eq(:value)
      expect(data).to eq([])
    end

    it 'now allows to read secret collection for user `random`' do
      tx, rx = s.read('secrets', 'key',
        :authorization => UIDS[:random]
      )
      id, data = rx.recv()

      expect(id).to eq(:value)
      expect(data).to eq(['ks/Os51X2RTtixTQ43ZD3geXrlY='])
    end

    it 'still forbids to write into secret collection for user `random`' do
      tx, rx = s.write('secrets', 'key', '00000000', [],
        :authorization => UIDS[:random]
      )
      id, data = rx.recv()

      expect(id).to eq(:error)
      expect(data).to eq([[12, 13], 'Permission denied'])
    end

    it 'still forbids to remove from secret collection for user `random`' do
      tx, rx = s.remove('secrets', 'key',
        :authorization => UIDS[:random]
      )
      id, data = rx.recv()

      expect(id).to eq(:error)
      expect(data).to eq([[12, 13], 'Permission denied'])
    end
  end
end
